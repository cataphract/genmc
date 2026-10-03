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

#include "EliminateAnnotationsPass.hpp"
#include "genmc/ADT/VSet.hpp"
#include "genmc/Support/Error.hpp"
#include "passes/InternalFunctions.hpp"
#include "passes/LLVMUtils.hpp"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/STLFunctionalExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/Casting.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

using namespace llvm;
using AnnotationOptions = EliminateAnnotationsPass::AnnotationOptions;

#define POSTDOM_PASS PostDominatorTreeWrapperPass
#define GET_POSTDOM_PASS() getAnalysis<POSTDOM_PASS>().getPostDomTree();

static auto isAnnotationBegin(Instruction *i) -> bool
{
	auto *ci = llvm::dyn_cast<CallInst>(i);
	if (!ci)
		return false;

	auto name = getCalledFunOrStripValName(*ci);
	return isInternalFunction(name) &&
	       internalFunNames.at(name) == InternalFunctions::AnnotateBegin;
}

static auto isAnnotationEnd(Instruction *i) -> bool
{
	auto *ci = llvm::dyn_cast<CallInst>(i);
	if (!ci)
		return false;

	auto name = getCalledFunOrStripValName(*ci);
	return isInternalFunction(name) &&
	       internalFunNames.at(name) == InternalFunctions::AnnotateEnd;
}

static auto isMutexCall(Instruction *i) -> bool
{
	auto *ci = llvm::dyn_cast<CallInst>(i);
	if (!ci)
		return false;

	auto name = getCalledFunOrStripValName(*ci);
	return isInternalFunction(name) && isMutexCode(internalFunNames.at(name));
}

/* We only annotate atomic instructions and selected intrinsics (e.g., locks) */
static auto isAnnotatable(Instruction *i) -> bool
{
	return (i->isAtomic() && !isa<FenceInst>(i)) || isMutexCall(i);
}

static auto getAnnotationValue(CallInst *ci) -> uint64_t
{
	auto *funArg = llvm::dyn_cast<ConstantInt>(ci->getOperand(0));
	VERIFY(funArg);
	return funArg->getValue().getLimitedValue();
}

static auto shouldAnnotate(const AnnotationOptions &options, uint64_t annotType) -> bool
{
	auto isHelperAnnot = [](uint64_t annotType) {
		return annotType == GENMC_KIND_HELPED || annotType == GENMC_KIND_HELPING;
	};
	auto isConfAnnot = [](uint64_t annotType) {
		return annotType == GENMC_KIND_CONFIRM || annotType == GENMC_KIND_SPECUL;
	};

	if (isHelperAnnot(annotType) && !options.annotHelper)
		return false;
	if (isConfAnnot(annotType) && !options.annotConf)
		return false;
	if (annotType == GENMC_ATTR_FINAL && !options.annotFinal)
		return false;
	return true;
}

using BlockSet = SmallPtrSet<BasicBlock *, 16>;

/* Returns the blocks strictly inside the paths from FROM to TO (FROM != TO)
 * that repeat no block, or nullopt if there is no such path */
static auto getBlocksBetween(BasicBlock *from, BasicBlock *to, const DominatorTree &DT)
	-> std::optional<BlockSet>;

static auto annotateInstructions(CallInst *begin, CallInst *end, const DominatorTree &DT,
				 const AnnotationOptions &options) -> bool
{
	if (!begin || !end)
		return false;

	auto annotType = getAnnotationValue(begin);
	unsigned opcode = 0; /* no opcode == 0 in LLVM */
	auto annotate = [&](Instruction &i) {
		/* we only annotate atomics */
		if (!isAnnotatable(&i))
			return;
		if (!opcode)
			opcode = i.getOpcode();
		VERIFY(opcode == i.getOpcode()); /* annotations across paths must match */
		if (shouldAnnotate(options, annotType))
			annotateInstruction(&i, "genmc.attr", annotType);
	};

	/* Annotate what runs between the begin and the end on the paths from one
	 * to the other that repeat no block. The begin dominates the end, so it
	 * comes first if they share a block, and the path is then that block. */
	auto *from = begin->getParent();
	auto *to = end->getParent();
	if (from == to) {
		std::for_each(std::next(begin->getIterator()), end->getIterator(), annotate);
		return true;
	}
	auto between = getBlocksBetween(from, to, DT);
	if (!between)
		return true;
	std::for_each(std::next(begin->getIterator()), from->end(), annotate);
	for (auto &bb : *from->getParent())
		if (between->contains(&bb))
			std::for_each(bb.begin(), bb.end(), annotate);
	std::for_each(to->begin(), end->getIterator(), annotate);
	return true;
}

using EdgePredicate = function_ref<bool(BasicBlock *, BasicBlock *)>;

/* Returns the blocks that reach TARGET (or that TARGET reaches, if FORWARD)
 * over edges that ISUSABLE accepts */
static auto getBlocksReaching(BasicBlock *target, bool forward, EdgePredicate isUsable) -> BlockSet;

/* Returns whether the edges between BLOCKS that ISUSABLE accepts form a cycle */
static auto hasCycle(const BlockSet &blocks, EdgePredicate isUsable) -> bool;

/* Returns the blocks of CANDIDATES on the paths from FROM to TO that repeat no
 * block and take only edges that ISUSABLE accepts, or nullopt if there are too
 * many of these paths to walk */
static auto walkBlocksBetween(BasicBlock *from, BasicBlock *to, const BlockSet &candidates,
			      EdgePredicate isUsable) -> std::optional<BlockSet>;

static auto getBlocksBetween(BasicBlock *from, BasicBlock *to, const DominatorTree &DT)
	-> std::optional<BlockSet>
{
	/*
	 * A path that repeats no block does not enter FROM, nor leave TO. If FROM
	 * dominates TO, it does not take a back edge U -> H (where H dominates U)
	 * either. H, which is not FROM, reaches TO along the rest of the path
	 * without going through FROM, so FROM dominates H, and H does not
	 * dominate FROM. Some path from the entry thus reaches FROM without
	 * visiting H, so the path must visit H between FROM and U already.
	 */
	auto pruneBackEdges = DT.isReachableFromEntry(to) && DT.dominates(from, to);
	auto isUsable = [&](BasicBlock *src, BasicBlock *dst) {
		return dst != from && src != to && !(pruneBackEdges && DT.dominates(dst, src));
	};

	/* The blocks inside the paths are among those that FROM reaches, and
	 * that reach TO */
	auto reached = getBlocksReaching(from, /*forward=*/true, isUsable);
	if (!reached.contains(to))
		return std::nullopt;
	BlockSet between;
	for (auto *bb : getBlocksReaching(to, /*forward=*/false, isUsable))
		if (bb != from && bb != to && reached.contains(bb))
			between.insert(bb);

	/*
	 * Without back edges, the edges between these blocks form no cycle,
	 * unless the CFG is irreducible. Each of the blocks then lies inside a
	 * path: following a path from FROM to it by one from it to TO repeats no
	 * block, as that would close a cycle.
	 */
	if (!hasCycle(between, isUsable))
		return between;

	/* Otherwise, find the blocks inside the paths by walking them */
	auto walked = walkBlocksBetween(from, to, between, isUsable);
	if (!walked)
		ERROR("Too many paths through annotated code with irreducible control flow in {}",
		      from->getParent()->getName().str());
	return walked;
}

static auto getBlocksReaching(BasicBlock *target, bool forward, EdgePredicate isUsable) -> BlockSet
{
	BlockSet blocks{target};
	SmallVector<BasicBlock *, 16> worklist{target};
	auto visit = [&](BasicBlock *bb) {
		if (blocks.insert(bb).second)
			worklist.push_back(bb);
	};
	while (!worklist.empty()) {
		auto *bb = worklist.pop_back_val();
		if (forward) {
			for (auto *succ : successors(bb))
				if (isUsable(bb, succ))
					visit(succ);
		} else {
			for (auto *pred : predecessors(bb))
				if (isUsable(pred, bb))
					visit(pred);
		}
	}
	return blocks;
}

static auto hasCycle(const BlockSet &blocks, EdgePredicate isUsable) -> bool
{
	/* Remove the blocks that no remaining block jumps to, until none is left */
	DenseMap<BasicBlock *, unsigned> preds;
	for (auto *bb : blocks)
		for (auto *succ : successors(bb))
			if (blocks.contains(succ) && isUsable(bb, succ))
				++preds[succ];
	SmallVector<BasicBlock *, 16> worklist;
	for (auto *bb : blocks)
		if (!preds.lookup(bb))
			worklist.push_back(bb);
	auto removed = 0U;
	while (!worklist.empty()) {
		auto *bb = worklist.pop_back_val();
		++removed;
		for (auto *succ : successors(bb))
			if (blocks.contains(succ) && isUsable(bb, succ) && !--preds[succ])
				worklist.push_back(succ);
	}
	return removed != blocks.size();
}

namespace {
/* Walks the paths of walkBlocksBetween() backward from their last block,
 * like foreachInBackPathTo(), as long as they are not too many */
class PathWalker {
public:
	PathWalker(BasicBlock *from, const BlockSet &candidates, EdgePredicate isUsable)
		: from(from), candidates(&candidates), isUsable(isUsable)
	{}

	/* Returns the blocks inside the paths from FROM to TO, or nullopt if
	 * walking them visits too many blocks */
	auto walk(BasicBlock *to) -> std::optional<BlockSet>
	{
		if (!walkBack(to))
			return std::nullopt;
		return std::move(walked);
	}

private:
	/* Bounds the blocks that a walk visits */
	static constexpr unsigned maxSteps = 1U << 20;

	/* Extends the current path, which starts at BB, backward */
	auto walkBack(BasicBlock *bb) -> bool;

	BasicBlock *from;
	const BlockSet *candidates;
	EdgePredicate isUsable;

	/* The current path, backward from TO, without TO */
	SmallVector<BasicBlock *, 16> path;
	BlockSet onPath;
	BlockSet walked;
	unsigned steps = maxSteps;
};
} // namespace

static auto walkBlocksBetween(BasicBlock *from, BasicBlock *to, const BlockSet &candidates,
			      EdgePredicate isUsable) -> std::optional<BlockSet>
{
	return PathWalker(from, candidates, isUsable).walk(to);
}

auto PathWalker::walkBack(BasicBlock *bb) -> bool
{
	BlockSet preds;
	for (auto *pred : predecessors(bb)) {
		if (!preds.insert(pred).second || !isUsable(pred, bb))
			continue;
		if (pred == from) {
			if (steps < path.size())
				return false;
			steps -= path.size();
			walked.insert(path.begin(), path.end());
			continue;
		}
		if (!candidates->contains(pred) || onPath.contains(pred))
			continue;
		if (steps-- == 0)
			return false;
		path.push_back(pred);
		onPath.insert(pred);
		auto done = walkBack(pred);
		onPath.erase(pred);
		path.pop_back();
		if (!done)
			return false;
	}
	return true;
}

static auto findMatchingEnd(CallInst *begin, const std::vector<CallInst *> &ends, DominatorTree &DT,
			    PostDominatorTree &PDT) -> CallInst *
{
	auto it = std::ranges::find_if(ends, [&](auto *ei) {
		return getAnnotationValue(begin) == getAnnotationValue(ei) &&
		       DT.dominates(begin, ei) &&
		       std::none_of(ends.begin(), ends.end(), [&](auto *ei2) {
			       return ei != ei2 && DT.dominates(begin, ei2) &&
				      PDT.dominates(ei2->getParent(), begin->getParent()) &&
				      DT.dominates(ei2, ei);
		       });
	});
	VERIFY(it != ends.end());
	return *it;
}

auto EliminateAnnotationsPass::run(Function &F, FunctionAnalysisManager &FAM) -> PreservedAnalyses
{
	std::vector<CallInst *> begins;
	std::vector<CallInst *> ends;

	for (auto &i : instructions(F)) {
		if (isAnnotationBegin(&i))
			begins.push_back(dyn_cast<CallInst>(&i));
		else if (isAnnotationEnd(&i))
			ends.push_back(dyn_cast<CallInst>(&i));
	}

	auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);
	auto &PDT = FAM.getResult<PostDominatorTreeAnalysis>(F);
	VSet<Instruction *> toDelete;

	auto changed = false;
	for (auto *bi : begins) {
		auto *ei = findMatchingEnd(bi, ends, DT, PDT);
		VERIFY(ei);
		changed |= annotateInstructions(bi, ei, DT, getOptions());
		toDelete.insert(bi);
		toDelete.insert(ei);
	}
	for (auto *i : toDelete) {
		i->eraseFromParent();
		changed = true;
	}
	return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
