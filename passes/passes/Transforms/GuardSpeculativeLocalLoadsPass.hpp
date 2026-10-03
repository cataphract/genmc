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

#ifndef GENMC_GUARD_SPECULATIVE_LOCAL_LOADS_PASS_HPP
#define GENMC_GUARD_SPECULATIVE_LOCAL_LOADS_PASS_HPP

#include <llvm/IR/PassManager.h>

/*
 * Clang may hoist the load of a union member above the discriminator test
 * that makes it meaningful, e.g., has_value && pointer != null in a
 * destructor. When the storage is thread-private, the inactive load is
 * unobservable, yet interpreting it can report mixed-size or uninitialized
 * reads. Executes such loads only when the discriminator selects them.
 * The IR cannot distinguish a hoisted load from an unconditional source
 * read, so the latter loses those diagnostics too.
 */
class GuardSpeculativeLocalLoadsPass : public llvm::PassInfoMixin<GuardSpeculativeLocalLoadsPass> {
public:
	auto run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) -> llvm::PreservedAnalyses;
};

#endif /* GENMC_GUARD_SPECULATIVE_LOCAL_LOADS_PASS_HPP */
