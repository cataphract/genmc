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

#include "FunctionInlinerPass.hpp"
#include "genmc/Support/Error.hpp"
#include "passes/InternalFunctions.hpp"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SCCIterator.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/Utils/Local.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/ValueHandle.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/CallPromotionUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/Local.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

using namespace llvm;

static auto promoteConstantDispatches(Module &M) -> bool;

/**
 * Do not inline any functions that are (mutually) recursive.
 *
 * Example transforms that are permitted (below only f1 will be inlined):
 * f->f1->f2->f3->f4->f2->f3->f4... => f->f2->f3->f4->f2->f3->f4
 * f->f1->f2->f2->f2->... => main->f2->f2->f2...
 */
static auto isRecursive(CallGraph &CG, Function &F) -> bool
{
	for (auto sccIt = scc_begin(&CG); !sccIt.isAtEnd(); ++sccIt) {
		if (std::ranges::find(*sccIt, CG[&F]) != sccIt->end() && sccIt.hasCycle())
			return true;
	}
	return false;
}

static auto isInlinable(CallGraph &CG, Function &F) -> bool
{
	return !F.isDeclaration() && !isInternalFunction(F.getName().str()) && !isRecursive(CG, F);
}

static auto inlineCall(CallBase *callBase) -> bool
{
	llvm::InlineFunctionInfo ifi;

	return InlineFunction(*callBase, ifi).isSuccess();
}

static auto inlineFunction(Module &M, Function *toInline) -> bool
{
	std::vector<CallBase *> calls;
	for (auto &F : M) {
		if (&F == toInline) /* No need to inline calls to itself */
			continue;

		for (auto &iit : instructions(F)) {
			if (!isa<InvokeInst>(&iit) && !isa<CallInst>(&iit))
				continue;

			auto *callBase = cast<CallBase>(&iit);
			if (callBase->getCalledFunction() == toInline)
				calls.push_back(callBase);
		}
	}

	auto changed = false;
	for (auto *ci : calls) {
		changed |= inlineCall(ci);
	}
	return changed;
}

auto FunctionInlinerPass::run(Module &M, ModuleAnalysisManager & /*AM*/) -> PreservedAnalyses
{
	/* Expose compiler-generated visitation calls before constructing the call
	 * graph. Inlining their bodies lets SROA eliminate mixed-width accesses to
	 * local union representations without changing shared-memory semantics. */
	auto changed = promoteConstantDispatches(M);
	CallGraph CG(M);

	for (auto &F : M) {
		/* Don't try on functions with empty bodies, external declarations, GenMCs own
		 * functions and (mutually) recursive functions */
		if (!F.empty() && isInlinable(CG, F))
			changed |= inlineFunction(M, &F);
	}
	return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

namespace {
/* A function stored in a constant dispatch table, and the byte offsets of the
 * table slots holding it */
struct DispatchTarget {
	Function *function;
	SmallVector<uint64_t, 1> slots;
};
} // namespace

using DispatchTargets = SmallVector<DispatchTarget, 4>;

/* Bound the code introduced by dispatch expansion: each target gets a direct
 * call, which is then inlined, and each slot a comparison */
constexpr unsigned maxDispatchTargets = 16;
constexpr unsigned maxDispatchSlots = 64;

/* A promotion adds the inlined code of its targets, including the dispatches
 * promoted in them, to every copy of its function, so nesting multiplies the
 * code that each level adds. Bound the instructions that the promotions of
 * each run of the pass add to the module after inlining. */
constexpr uint64_t maxDispatchGrowth = 1U << 15;

/* The interpreter allocates globals in this address space as GenMC-internal
 * memory, which user addresses cannot reach */
constexpr unsigned internalAddressSpace = 42;

namespace {
/* Estimates the instructions in each function once the inliner has run, and
 * how many copies of the function the module then holds: one, plus one in
 * each copy of a function that inlines it. A promotion adds its comparisons,
 * and the inlined code of each target, to every copy of its function. */
class InlinedSizeEstimate {
public:
	InlinedSizeEstimate(Module &M, CallGraph &CG);

	/* Returns how many instructions promoting CALL to TARGETS adds to the
	 * module after inlining. Promotions only ever increase it. */
	auto getPromotionGrowth(CallInst &call, const DispatchTargets &targets) -> uint64_t;

	/* Accounts for promoting CALL to TARGETS. The promotion must add its
	 * direct calls to the call graph before the next estimate. */
	void addPromotion(CallInst &call, const DispatchTargets &targets);

private:
	void update();
	[[nodiscard]] auto getInlinedCallee(const CallGraphNode::CallRecord &record) const
		-> Function *;

	Module &module;
	CallGraph &CG;
	SmallPtrSet<Function *, 16> inlinable;
	DenseMap<Function *, uint64_t> ownSize;
	DenseMap<Function *, uint64_t> inlinedSize;
	DenseMap<Function *, uint64_t> copies;
	bool stale = false;
};
} // namespace

static auto getDispatchTargets(CallInst &call, DispatchTargets &targets) -> bool;
static auto reachesCaller(CallGraph &CG, CallInst &call, const DispatchTargets &targets) -> bool;
static void promoteDispatch(CallGraph &CG, CallInst *call, const DispatchTargets &targets);

static auto promoteConstantDispatches(Module &M) -> bool
{
	/* A dispatch that always fails erases the rest of its block, which can
	 * hold later dispatches, or values that they use. Track the calls with
	 * handles that deletion clears, and collect their targets again before
	 * promoting them, which rejects calls through erased values. */
	SmallVector<WeakVH, 8> dispatches;
	for (auto &function : M)
		for (auto &instruction : instructions(function))
			if (auto *call = dyn_cast<CallInst>(&instruction);
			    call && call->isIndirectCall()) {
				DispatchTargets targets;
				if (getDispatchTargets(*call, targets))
					dispatches.emplace_back(call);
			}
	if (dispatches.empty())
		return false;

	/* The call graph has no edges for indirect calls, so a function that
	 * reaches itself only through a table is not recursive. Promoting that
	 * dispatch would make it recursive, and stop it from being inlined.
	 * Promoted calls join the graph, so later checks also see them. */
	CallGraph CG(M);
	InlinedSizeEstimate estimate(M, CG);

	/* Promote the cheapest dispatches first, so that a few costly ones do not
	 * exhaust the budget. Each dispatch is queued with a lower bound on its
	 * growth, and promoted once its current growth is the lowest one. A
	 * dispatch that always fails costs the least in its function, so it
	 * still erases the dispatches in its block before they are promoted. */
	using Candidate = std::pair<uint64_t, unsigned>;
	std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> candidates;
	for (auto i = 0U; i < dispatches.size(); ++i)
		candidates.emplace(0, i);
	auto budget = maxDispatchGrowth;
	auto changed = false;
	while (!candidates.empty()) {
		auto [bound, index] = candidates.top();
		candidates.pop();
		auto *call = cast_or_null<CallInst>(dispatches[index]);
		DispatchTargets targets;
		if (!call || !getDispatchTargets(*call, targets) ||
		    reachesCaller(CG, *call, targets))
			continue;
		auto growth = estimate.getPromotionGrowth(*call, targets);
		if (growth > bound) {
			candidates.emplace(growth, index);
			continue;
		}
		/* Every other dispatch adds at least as much */
		if (growth > budget)
			break;
		budget -= growth;
		estimate.addPromotion(*call, targets);
		promoteDispatch(CG, call, targets);
		changed = true;
	}
	return changed;
}

static auto collectDispatchTargets(Constant *value, uint64_t offset, CallInst &call,
				   DispatchTargets &targets, unsigned &slots) -> bool;

static auto getDispatchTargets(CallInst &call, DispatchTargets &targets) -> bool
{
	auto *load = dyn_cast<LoadInst>(call.getCalledOperand());
	if (!load || !load->isSimple() || call.isMustTailCall())
		return false;
	auto *gep = dyn_cast<GEPOperator>(load->getPointerOperand());
	if (!gep || !gep->isInBounds() || gep->getNumIndices() == 0 ||
	    gep->getResultElementType() != load->getType())
		return false;
	auto *global = dyn_cast<GlobalVariable>(gep->getPointerOperand());
	auto *firstIndex = dyn_cast<ConstantInt>(gep->getOperand(1));
	if (!global || !global->isConstant() || !global->hasDefinitiveInitializer() ||
	    !firstIndex || !firstIndex->isZero())
		return false;

	/* The dispatch computes the slot offset from these indices, and the
	 * instruction annotator only accepts integer constants */
	if (std::ranges::any_of(gep->indices(), [](const Use &index) {
		    return isa<Constant>(index.get()) && !isa<ConstantInt>(index.get());
	    }))
		return false;

	/* Clang can address the array inside a one-field table wrapper directly.
	 * Reject reinterpretations with a different stride or element layout. */
	auto *tableType = global->getValueType();
	while (tableType != gep->getSourceElementType()) {
		auto *wrapper = dyn_cast<StructType>(tableType);
		if (!wrapper || wrapper->getNumElements() != 1)
			return false;
		tableType = wrapper->getElementType(0);
	}
	auto slots = 0U;
	return collectDispatchTargets(global->getInitializer(), 0, call, targets, slots) &&
	       !targets.empty();
}

/* Collects the functions in VALUE, which is stored OFFSET bytes into the table */
static auto collectDispatchTargets(Constant *value, uint64_t offset, CallInst &call,
				   DispatchTargets &targets, unsigned &slots) -> bool
{
	if (auto *function = dyn_cast<Function>(value->stripPointerCasts())) {
		if (value->getType() != call.getCalledOperand()->getType() ||
		    function->getFunctionType() != call.getFunctionType() ||
		    function->getCallingConv() != call.getCallingConv() ||
		    !isLegalToPromote(call, function))
			return false;
		auto *target = std::ranges::find(targets, function, &DispatchTarget::function);
		if (target == targets.end())
			target = &targets.emplace_back(
				DispatchTarget{.function = function, .slots = {}});
		target->slots.push_back(offset);
		return targets.size() <= maxDispatchTargets && ++slots <= maxDispatchSlots;
	}

	const auto &DL = call.getModule()->getDataLayout();
	if (auto *array = dyn_cast<ConstantArray>(value)) {
		auto stride =
			DL.getTypeAllocSize(array->getType()->getElementType()).getFixedValue();
		for (auto i = 0U; i < array->getNumOperands(); ++i)
			if (!collectDispatchTargets(array->getOperand(i), offset + (i * stride),
						    call, targets, slots))
				return false;
		return true;
	}
	auto *record = dyn_cast<ConstantStruct>(value);
	if (!record)
		return false;
	const auto *layout = DL.getStructLayout(record->getType());
	for (auto i = 0U; i < record->getNumOperands(); ++i)
		if (!collectDispatchTargets(record->getOperand(i),
					    offset + layout->getElementOffset(i).getFixedValue(),
					    call, targets, slots))
			return false;
	return true;
}

/* Returns whether CALL's function is reachable from TARGETS in the call graph */
static auto reachesCaller(CallGraph &CG, CallInst &call, const DispatchTargets &targets) -> bool
{
	auto *caller = CG[call.getFunction()];
	SmallPtrSet<CallGraphNode *, 16> visited;
	SmallVector<CallGraphNode *, 16> worklist;
	for (const auto &target : targets)
		worklist.push_back(CG[target.function]);
	while (!worklist.empty()) {
		auto *node = worklist.pop_back_val();
		if (node == caller)
			return true;
		if (!visited.insert(node).second)
			continue;
		for (auto &record : *node)
			worklist.push_back(record.second);
	}
	return false;
}

InlinedSizeEstimate::InlinedSizeEstimate(Module &M, CallGraph &CG) : module(M), CG(CG)
{
	/* The functions that isInlinable accepts, finding the recursive ones in
	 * a single pass. Promotions never close a cycle, so this stays valid. */
	SmallPtrSet<Function *, 8> recursive;
	for (auto scc = scc_begin(&CG); !scc.isAtEnd(); ++scc)
		if (scc.hasCycle())
			for (auto *node : *scc)
				if (auto *function = node->getFunction())
					recursive.insert(function);
	for (auto &function : M) {
		if (function.isDeclaration())
			continue;
		/* Inlining scopes each alloca of the callee with lifetime markers */
		auto size = uint64_t(function.getInstructionCount());
		for (auto &inst : function.getEntryBlock())
			if (isa<AllocaInst>(inst))
				size += 2;
		ownSize[&function] = size;
		if (!isInternalFunction(function.getName().str()) && !recursive.contains(&function))
			inlinable.insert(&function);
	}
	update();
}

static auto isPromotedTarget(CallInst &call, const DispatchTarget &target) -> bool;
static auto getPromotionSize(CallInst &call, const DispatchTargets &targets) -> uint64_t;

auto InlinedSizeEstimate::getPromotionGrowth(CallInst &call, const DispatchTargets &targets)
	-> uint64_t
{
	if (stale)
		update();
	auto growth = getPromotionSize(call, targets);
	for (const auto &target : targets)
		if (inlinable.contains(target.function) && isPromotedTarget(call, target))
			growth = SaturatingAdd(growth, inlinedSize.lookup(target.function));
	return SaturatingMultiply(growth, copies.lookup(call.getFunction()));
}

void InlinedSizeEstimate::addPromotion(CallInst &call, const DispatchTargets &targets)
{
	auto *caller = call.getFunction();
	ownSize[caller] = SaturatingAdd(ownSize.lookup(caller), getPromotionSize(call, targets));
	stale = true;
}

void InlinedSizeEstimate::update()
{
	/* Order the functions so that inlined callees precede their callers.
	 * Recursive functions are not inlined, so the inlined calls form a DAG. */
	SmallVector<Function *, 32> order;
	SmallPtrSet<Function *, 32> visited;
	SmallVector<std::pair<Function *, CallGraphNode::iterator>, 8> stack;
	for (auto &root : module) {
		if (root.isDeclaration() || !visited.insert(&root).second)
			continue;
		stack.emplace_back(&root, CG[&root]->begin());
		while (!stack.empty()) {
			auto [function, next] = stack.back();
			if (next == CG[function]->end()) {
				order.push_back(function);
				stack.pop_back();
				continue;
			}
			++stack.back().second;
			if (auto *callee = getInlinedCallee(*next);
			    callee && visited.insert(callee).second)
				stack.emplace_back(callee, CG[callee]->begin());
		}
	}

	for (auto *function : order) {
		auto size = ownSize.lookup(function);
		for (const auto &record : *CG[function])
			if (auto *callee = getInlinedCallee(record))
				size = SaturatingAdd(size, inlinedSize.lookup(callee));
		inlinedSize[function] = size;
	}
	copies.clear();
	for (auto *function : reverse(order)) {
		auto count = SaturatingAdd(copies.lookup(function), uint64_t(1));
		copies[function] = count;
		for (const auto &record : *CG[function])
			if (auto *callee = getInlinedCallee(record))
				copies[callee] = SaturatingAdd(copies.lookup(callee), count);
	}
	stale = false;
}

/* Returns the function that the inliner inlines into the call of RECORD, if
 * any. Records without a call stand for callbacks, which are not inlined.
 * Calls erased by failing dispatches still count, which only overestimates,
 * and keeps the growth of other promotions from decreasing. */
auto InlinedSizeEstimate::getInlinedCallee(const CallGraphNode::CallRecord &record) const
	-> Function *
{
	auto *callee = record.second->getFunction();
	return record.first && callee && inlinable.contains(callee) ? callee : nullptr;
}

/* Returns the instructions that promoting CALL to TARGETS adds to its function:
 * the offset and the failure, and the comparisons, branches, direct call and
 * result of each target that it calls */
static auto getPromotionSize(CallInst &call, const DispatchTargets &targets) -> uint64_t
{
	auto size = uint64_t(4);
	for (const auto &target : targets)
		if (isPromotedTarget(call, target))
			size += (2 * target.slots.size()) + 4;
	return size;
}

/* Returns whether promoting CALL calls TARGET directly: a constant offset
 * selects at most one target statically */
static auto isPromotedTarget(CallInst &call, const DispatchTarget &target) -> bool
{
	auto *load = cast<LoadInst>(call.getCalledOperand());
	auto *gep = cast<GEPOperator>(load->getPointerOperand());
	const auto &DL = call.getModule()->getDataLayout();
	APInt offset(DL.getIndexTypeSizeInBits(gep->getType()), 0);
	return !gep->accumulateConstantOffset(DL, offset) ||
	       is_contained(target.slots, offset.getLimitedValue());
}

static void promoteDispatchTarget(CallGraph &CG, CallInst *call, Value *selected, Function *target);
static void promoteToDirectCall(CallGraph &CG, CallInst &call, Function *target);
static void replaceWithDispatchFailure(CallInst *call);

static void promoteDispatch(CallGraph &CG, CallInst *call, const DispatchTargets &targets)
{
	/* Inlinable calls in a function with debug info need a location.
	 * The direct calls inherit it from the indirect one. */
	if (!call->getDebugLoc())
		if (auto *scope = call->getFunction()->getSubprogram())
			call->setDebugLoc(DILocation::get(call->getContext(), 0, 0, scope));

	/* Select the slot by its integer offset in the table: the instruction
	 * annotator only accepts null pointer constants, and the offset does not
	 * depend on the host representation of the function pointer stored in
	 * the slot, which differs when target and host endianness do. Omit wrap
	 * flags, so that an out-of-bounds index reaches the fallback instead of
	 * making the offset poison. */
	auto *load = cast<LoadInst>(call->getCalledOperand());
	IRBuilder<> builder(call);
	auto *offset = emitGEPOffset(&builder, call->getModule()->getDataLayout(),
				     cast<GEPOperator>(load->getPointerOperand()),
				     /*NoAssumptions=*/true);
	for (const auto &target : targets) {
		builder.SetInsertPoint(call);
		Value *selected = nullptr;
		for (auto slot : target.slots) {
			auto *matches = builder.CreateICmpEQ(
				offset, ConstantInt::get(offset->getType(), slot));
			selected = selected ? builder.CreateOr(selected, matches) : matches;
		}
		/* A constant offset selects its target statically */
		if (auto *known = dyn_cast<ConstantInt>(selected)) {
			if (known->isZero())
				continue;
			promoteToDirectCall(CG, *call, target.function);
			return;
		}
		promoteDispatchTarget(CG, call, selected, target.function);
	}
	/* Every defined load from this immutable table selects a collected
	 * target. Reaching the fallback means the index was out of bounds,
	 * which the interpreter would otherwise turn into a call through an
	 * arbitrary pointer. */
	replaceWithDispatchFailure(call);
}

/* Calls TARGET directly where SELECTED holds, and leaves CALL to the other cases */
static void promoteDispatchTarget(CallGraph &CG, CallInst *call, Value *selected, Function *target)
{
	Instruction *thenTerm;
	Instruction *elseTerm;
	SplitBlockAndInsertIfThenElse(selected, call, &thenTerm, &elseTerm);
	auto *merge = call->getParent();
	auto *direct = cast<CallInst>(call->clone());
	direct->insertBefore(thenTerm->getIterator());
	promoteToDirectCall(CG, *direct, target);
	call->moveBefore(*elseTerm->getParent(), elseTerm->getIterator());
	if (!call->getType()->isVoidTy()) {
		auto *result =
			PHINode::Create(call->getType(), 2, "dispatch.result", merge->begin());
		call->replaceAllUsesWith(result);
		result->addIncoming(direct, thenTerm->getParent());
		result->addIncoming(call, elseTerm->getParent());
	}
}

/* Makes CALL call TARGET, and adds the call to the call graph */
static void promoteToDirectCall(CallGraph &CG, CallInst &call, Function *target)
{
	promoteCall(call, target);
	CG[call.getFunction()]->addCalledFunction(&call, CG[target]);
	/* The verifier rejects inlinable calls without a location in functions
	 * with debug info, but GenMC only runs it in builds with assertions */
	VERIFY(call.getDebugLoc() || !call.getFunction()->getSubprogram());
}

static auto getDispatchString(Module &M, StringRef name, StringRef text) -> Constant *;

static void replaceWithDispatchFailure(CallInst *call)
{
	auto &M = *call->getModule();
	IRBuilder<> builder(call);
	auto *stringType = builder.getPtrTy(internalAddressSpace);
	auto failure = M.getOrInsertFunction("__VERIFIER_assert_fail", builder.getVoidTy(),
					     stringType, stringType, builder.getInt32Ty());
	builder.CreateCall(failure,
			   {getDispatchString(M, "__genmc_dispatch_bounds",
					      "indirect call through an out-of-bounds dispatch "
					      "table entry"),
			    getDispatchString(M, "__genmc_dispatch_file", "<dispatch table>"),
			    builder.getInt32(0)});
	/* Keep the load of the callee, even if nothing uses it: GenMC reports
	 * invalid accesses and races on it, also on paths that never call. */
	changeToUnreachable(call);
}

/* Returns a string that the dispatch failures of M share. Globals are laid out
 * in module order, so a user-visible string would become valid memory right
 * past the end of the last global, where out-of-bounds table reads land. */
static auto getDispatchString(Module &M, StringRef name, StringRef text) -> Constant *
{
	auto *initializer = ConstantDataArray::getString(M.getContext(), text);
	auto *string = M.getNamedGlobal(name);
	if (string && string->isConstant() && string->hasDefinitiveInitializer() &&
	    string->getInitializer() == initializer &&
	    string->getAddressSpace() == internalAddressSpace)
		return string;
	string = new GlobalVariable(M, initializer->getType(), /*isConstant=*/true,
				    GlobalValue::PrivateLinkage, initializer, name,
				    /*InsertBefore=*/nullptr, GlobalValue::NotThreadLocal,
				    internalAddressSpace);
	string->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
	string->setAlignment(Align(1));
	return string;
}
