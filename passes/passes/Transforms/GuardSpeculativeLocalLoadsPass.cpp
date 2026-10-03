/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 *
 * Apache License 2.0:
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * MIT License:
 *     https://opensource.org/licenses/MIT
 */

#include "GuardSpeculativeLocalLoadsPass.hpp"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/CaptureTracking.h>
#include <llvm/Analysis/DomTreeUpdater.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PatternMatch.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Local.h>

#include <cstdint>
#include <utility>

using namespace llvm;
using llvm::PatternMatch::match;

using GuardedLoad = std::pair<LoadInst *, Value *>;

namespace {
/* Analyses shared by all candidate loads of a function. Capture tracking
 * walks every use of an alloca, so its result is computed once per alloca. */
struct GuardContext {
	explicit GuardContext(DominatorTree &DT) : DT(DT) {}

	DominatorTree &DT;
	DenseMap<const AllocaInst *, bool> captured;
};
} // namespace

static void findGuardedLoads(SelectInst &select, GuardContext &ctx,
			     SmallVectorImpl<GuardedLoad> &loads);
static void collectLoads(Value *pointer, Value *guard, BranchInst *branch, GuardContext &ctx,
			 SmallPtrSetImpl<Value *> &visited, SmallVectorImpl<GuardedLoad> &loads,
			 bool root = false);
static void guardLoad(LoadInst *load, Value *guard, DomTreeUpdater &DTU);
static auto isGuardedBy(LoadInst *load, Value *guard, DominatorTree &DT) -> bool;
static auto makeGuardAvailable(LoadInst *load, Value *guard, BranchInst *branch, GuardContext &ctx)
	-> bool;
static auto isEventFreeUntil(Instruction *point, BranchInst *branch, GuardContext &ctx) -> bool;
static auto isPrivateLoad(LoadInst *load, GuardContext &ctx) -> bool;

auto GuardSpeculativeLocalLoadsPass::run(Function &F, FunctionAnalysisManager &FAM)
	-> PreservedAnalyses
{
	GuardContext ctx(FAM.getResult<DominatorTreeAnalysis>(F));
	SmallVector<GuardedLoad, 8> loads;
	for (auto &instruction : instructions(F))
		if (auto *select = dyn_cast<SelectInst>(&instruction))
			findGuardedLoads(*select, ctx, loads);
	DomTreeUpdater DTU(ctx.DT, DomTreeUpdater::UpdateStrategy::Eager);
	SmallPtrSet<LoadInst *, 8> guarded;
	for (auto [load, guard] : loads)
		if (guarded.insert(load).second)
			guardLoad(load, guard, DTU);
	return loads.empty() ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

static void findGuardedLoads(SelectInst &select, GuardContext &ctx,
			     SmallVectorImpl<GuardedLoad> &loads)
{
	/* Match the short-circuit destructor test: has_value && pointer != null.
	 * Every other pointer use must lie under the true branch of that test. */
	if (!select.hasOneUse() || !match(select.getFalseValue(), PatternMatch::m_Zero()))
		return;
	auto *branch = dyn_cast<BranchInst>(*select.user_begin());
	auto *comparison = dyn_cast<ICmpInst>(select.getTrueValue());
	if (!branch || !branch->isConditional() || branch->getCondition() != &select ||
	    !comparison || !comparison->hasOneUse() ||
	    comparison->getPredicate() != ICmpInst::ICMP_NE ||
	    !isa<ConstantPointerNull>(comparison->getOperand(1)))
		return;
	auto *active = branch->getSuccessor(0);
	if (active == branch->getSuccessor(1) ||
	    active->getSinglePredecessor() != branch->getParent())
		return;
	/* Only loads and phis are guarded. Unlike constants, all their users lie in
	 * this function, so the dominator tree can place them. */
	auto *pointer = dyn_cast<Instruction>(comparison->getOperand(0));
	if (!pointer)
		return;
	for (auto *user : pointer->users()) {
		if (user == comparison)
			continue;
		auto *instruction = dyn_cast<Instruction>(user);
		if (!instruction || !ctx.DT.dominates(active, instruction->getParent()))
			return;
	}
	SmallPtrSet<Value *, 8> visited;
	collectLoads(pointer, select.getCondition(), branch, ctx, visited, loads, true);
}

static void collectLoads(Value *pointer, Value *guard, BranchInst *branch, GuardContext &ctx,
			 SmallPtrSetImpl<Value *> &visited, SmallVectorImpl<GuardedLoad> &loads,
			 bool root)
{
	if (match(guard, PatternMatch::m_One()) || !visited.insert(pointer).second ||
	    (!root && !pointer->hasOneUse()))
		return;
	if (auto *load = dyn_cast<LoadInst>(pointer)) {
		/* !noundef makes an undefined representation UB at the load itself,
		 * even when a later select would mask the value. */
		if (load->getMetadata(LLVMContext::MD_noundef) || !isPrivateLoad(load, ctx))
			return;
		if (match(guard, PatternMatch::m_Zero()) ||
		    (!isGuardedBy(load, guard, ctx.DT) &&
		     makeGuardAvailable(load, guard, branch, ctx)))
			loads.emplace_back(load, guard);
		return;
	}
	/* Clang can merge the discriminator and union pointer separately. Match
	 * incoming edges to recover the discriminator at the speculative load. */
	auto *pointerPhi = dyn_cast<PHINode>(pointer);
	auto *guardPhi = dyn_cast<PHINode>(guard);
	if (!pointerPhi || !guardPhi || pointerPhi->getParent() != guardPhi->getParent())
		return;
	for (unsigned i = 0; i < pointerPhi->getNumIncomingValues(); ++i) {
		auto *incoming = pointerPhi->getIncomingBlock(i);
		collectLoads(pointerPhi->getIncomingValue(i),
			     guardPhi->getIncomingValueForBlock(incoming), branch, ctx, visited,
			     loads);
	}
}

static void guardLoad(LoadInst *load, Value *guard, DomTreeUpdater &DTU)
{
	/* The inactive representation is unobservable: the select masks its only
	 * comparison and all remaining uses are dominated by the active branch.
	 * Keep the original load at its original point whenever it is active. */
	auto *inactive = Constant::getNullValue(load->getType());
	if (match(guard, PatternMatch::m_Zero())) {
		load->replaceAllUsesWith(inactive);
		load->eraseFromParent();
		return;
	}
	auto *before = load->getParent();
	auto *thenTerm =
		SplitBlockAndInsertIfThen(guard, load->getIterator(), false, nullptr, &DTU);
	auto *after = load->getParent();
	load->moveBefore(*thenTerm->getParent(), thenTerm->getIterator());
	auto *value = PHINode::Create(load->getType(), 2, "guarded.local", after->begin());
	/* A delayed load can leave debug users ahead of the merge. Salvage those
	 * instead of pointing them at the phi before its definition. */
	replaceAllDbgUsesWith(*load, *value, *value, DTU.getDomTree());
	load->replaceAllUsesWith(value);
	value->addIncoming(load, thenTerm->getParent());
	value->addIncoming(inactive, before);
}

/* Whether LOAD already executes only when GUARD holds. Jump threading moves
 * an earlier guarded load under the discriminator branch itself; guarding it
 * again would only add a redundant branch. */
static auto isGuardedBy(LoadInst *load, Value *guard, DominatorTree &DT) -> bool
{
	if (isa<Constant>(guard))
		return false;
	return any_of(guard->users(), [&](User *user) {
		auto *branch = dyn_cast<BranchInst>(user);
		return branch && branch->isConditional() && branch->getCondition() == guard &&
		       DT.dominates(BasicBlockEdge(branch->getParent(), branch->getSuccessor(0)),
				    load->getParent());
	});
}

static auto makeGuardAvailable(LoadInst *load, Value *guard, BranchInst *branch, GuardContext &ctx)
	-> bool
{
	/* Events between the load and the original branch would gain a control
	 * dependency on the discriminator, and IMM orders writes after the reads
	 * they depend on. Only private reads may lie in between. */
	if (!isEventFreeUntil(load, branch, ctx))
		return false;
	if (ctx.DT.dominates(guard, load))
		return true;
	auto *definition = dyn_cast<Instruction>(guard);
	if (!definition || definition->getParent() != load->getParent() ||
	    !load->comesBefore(definition))
		return false;
	for (auto &use : load->uses())
		if (!ctx.DT.dominates(definition, use))
			return false;

	/* A common cleanup block can load the union before its discriminator.
	 * The scan above admits only pure operations and private reads up to the
	 * original branch, so delaying the union load preserves every write and
	 * observable access. The discriminator still executes unconditionally,
	 * including any !noundef requirement. */
	load->moveBefore(*definition->getParent(), definition->getNextNode()->getIterator());
	/* Only instruction order changed; the block dominator tree remains valid. */
	return true;
}

/* Whether the instructions after POINT up to BRANCH, following unconditional
 * branches, contain no events other than private reads. A branch inserted at
 * POINT then adds control dependencies only to those reads; later events
 * already depend on the guard through BRANCH. */
static auto isEventFreeUntil(Instruction *point, BranchInst *branch, GuardContext &ctx) -> bool
{
	SmallPtrSet<BasicBlock *, 8> visited;
	visited.insert(point->getParent());
	for (auto *instruction = point->getNextNode(); instruction != branch;) {
		if (auto *jump = dyn_cast<BranchInst>(instruction);
		    jump && jump->isUnconditional()) {
			auto *next = jump->getSuccessor(0);
			if (!visited.insert(next).second)
				return false;
			instruction = &*next->getFirstNonPHIIt();
			continue;
		}
		if (auto *read = dyn_cast<LoadInst>(instruction)) {
			if (!isPrivateLoad(read, ctx))
				return false;
		} else if (instruction->mayReadOrWriteMemory() ||
			   instruction->mayHaveSideEffects() || instruction->isTerminator() ||
			   isa<CallBase, AllocaInst>(instruction)) {
			return false;
		}
		instruction = instruction->getNextNode();
	}
	return true;
}

static auto isPrivateLoad(LoadInst *load, GuardContext &ctx) -> bool
{
	if (!load->isSimple())
		return false;
	const auto &layout = load->getModule()->getDataLayout();
	int64_t offset = 0;
	auto *allocation = dyn_cast<AllocaInst>(
		GetPointerBaseWithConstantOffset(load->getPointerOperand(), offset, layout));
	if (!allocation || offset < 0 || allocation->getAlign() < load->getAlign() ||
	    uint64_t(offset) % load->getAlign().value() != 0)
		return false;
	auto bytes = allocation->getAllocationSize(layout);
	auto width = layout.getTypeStoreSize(load->getType());
	if (!bytes || bytes->isScalable() || width.isScalable() ||
	    uint64_t(offset) > bytes->getFixedValue() ||
	    width.getFixedValue() > bytes->getFixedValue() - uint64_t(offset))
		return false;
	auto [entry, inserted] = ctx.captured.try_emplace(allocation, false);
	if (inserted) {
#if LLVM_VERSION_MAJOR >= 21
		entry->second = PointerMayBeCaptured(allocation, true, 4096);
#else
		entry->second = PointerMayBeCaptured(allocation, true, true, 4096);
#endif
	}
	return !entry->second;
}
