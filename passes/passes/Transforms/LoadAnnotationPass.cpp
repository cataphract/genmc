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

#include "LoadAnnotationPass.hpp"
#include "genmc/Execution/LoadAnnotation.hpp"
#include "genmc/Support/Error.hpp"
#include "passes/InternalFunctions.hpp"
#include "passes/LLVMUtils.hpp"
#include "passes/Transforms/InstAnnotator.hpp"

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/Casting.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

using namespace llvm;

/*
 * Returns the source loads of an assume statement, that is,
 * loads the result of which is used in the assume.
 */
static auto getSourceLoads(CallInst *assm) -> std::vector<Instruction *>
{
	std::vector<Instruction *> source;

	/* The values that the assume depends on form a graph, which may have
	 * cycles through Φs. Visit each value once: following each path instead
	 * takes exponential time after inlining a sequence of dispatches. */
	SmallPtrSet<Instruction *, 16> visited;
	SmallVector<Instruction *, 16> worklist;
	auto visit = [&](Value *v) {
		if (auto *i = dyn_cast<Instruction>(v); i && visited.insert(i).second)
			worklist.push_back(i);
	};
	visit(assm->getOperand(0));
	while (!worklist.empty()) {
		auto *i = worklist.pop_back_val();

		/* Don't go past stores or allocas (CASes are OK) */
		if (isa<StoreInst>(i) || isa<AtomicRMWInst>(i) || isa<AllocaInst>(i))
			continue;

		/* If we reached a (source) load, collect it */
		if (isa<LoadInst>(i) || isa<AtomicCmpXchgInst>(i)) {
			source.push_back(i);
			continue;
		}

		for (auto &use : i->operands()) {
			if (isa<Instruction>(use.get())) {
				visit(use.get());
				continue;
			}
			/* A constant that a Φ selects depends on the branch to it */
			auto *phi = dyn_cast<PHINode>(i);
			if (!phi || !isa<Constant>(use.get()))
				continue;
			auto *bi =
				dyn_cast<BranchInst>(phi->getIncomingBlock(use)->getTerminator());
			if (bi && bi->isConditional())
				visit(bi->getCondition());
		}
	}
	std::ranges::sort(source);
	return source;
}

/* Returns whether some path from LOAD to ASSM runs no instruction other than
 * LOAD that keeps LOAD from being annotated */
static auto hasCleanPathToAssume(Instruction *load, CallInst *assm) -> bool;

/*
 * Given an assume's source loads, returns the annotatable ones.
 */
static auto filterAnnotatableFromSource(CallInst *assm, const std::vector<Instruction *> &source)
	-> std::vector<Instruction *>
{
	std::vector<Instruction *> result;

	/* Collect candidates for which the path to the assume is clear */
	std::ranges::copy_if(source, std::back_inserter(result),
			     [&](auto *li) { return hasCleanPathToAssume(li, assm); });
	return result;
}

/* Returns whether I, if run between a load and an assume, keeps the load
 * from being annotated */
static auto blocksAnnotation(Instruction &i) -> bool;

static auto hasCleanPathToAssume(Instruction *load, CallInst *assm) -> bool
{
	auto isClean = [](auto &&insts) { return llvm::none_of(insts, blocksAnnotation); };
	auto *from = load->getParent();
	auto *to = assm->getParent();

	/* Only paths that repeat no block count. Within a single block, the load
	 * must thus precede the assume. */
	if (from == to)
		return load->comesBefore(assm) &&
		       isClean(make_range(std::next(load->getIterator()), assm->getIterator()));
	if (!isClean(make_range(std::next(load->getIterator()), from->end())) ||
	    !isClean(make_range(to->begin(), assm->getIterator())))
		return false;

	/* Removing the cycles of a path through clean blocks leaves one that
	 * repeats no block, so search for any path through clean blocks rather
	 * than enumerating the paths that repeat no block */
	SmallPtrSet<BasicBlock *, 16> visited{from};
	SmallVector<BasicBlock *, 16> worklist(successors(from));
	while (!worklist.empty()) {
		auto *bb = worklist.pop_back_val();
		if (bb == to)
			return true;
		if (visited.insert(bb).second && isClean(*bb))
			llvm::append_range(worklist, successors(bb));
	}
	return false;
}

static auto blocksAnnotation(Instruction &i) -> bool
{
	return hasSideEffects(&i) /* also CASes */ || isa<LoadInst>(&i);
}

/*
 * Returns all of ASSM's annotatable loads
 */
static auto getAnnotatableLoads(CallInst *assm) -> std::vector<Instruction *>
{
	if (!isAssumeFunction(getCalledFunOrStripValName(*assm)))
		return {}; /* yet another check... */

	auto sourceLoads = getSourceLoads(assm);
	return filterAnnotatableFromSource(assm, sourceLoads);
}

static auto extractAssumeArgument(CallInst *assume) -> uint64_t
{
	auto *arg = dyn_cast<Constant>(assume->getArgOperand(1));
	VERIFY(arg && arg->getType()->isIntegerTy());

	return arg->getUniqueInteger().getLimitedValue();
}

auto LoadAnnotationAnalysis::run(Function &F, FunctionAnalysisManager & /*FAM*/) -> Result
{
	InstAnnotator annotator;

	for (auto &i : instructions(F)) {
		auto *call = llvm::dyn_cast<llvm::CallInst>(&i);
		if (call && isAssumeFunction(getCalledFunOrStripValName(*call))) {
			auto loads = getAnnotatableLoads(call);
			for (auto *l : loads) {
				auto rawType = extractAssumeArgument(call);
				VERIFY(rawType <= static_cast<uint64_t>(AssumeType::Spinloop));
				auto type = static_cast<AssumeType>(rawType);
				/* Leave loads with too large an annotation unannotated */
				if (auto annot = annotator.annotate(l))
					result_.annotMap[l] =
						std::make_pair(type, std::move(annot));
			}
		}
	}
	return result_;
}

auto LoadAnnotationPass::run(Function &F, FunctionAnalysisManager &FAM) -> PreservedAnalyses
{
	LAI_ = FAM.getResult<LoadAnnotationAnalysis>(F);
	return PreservedAnalyses::all();
}
