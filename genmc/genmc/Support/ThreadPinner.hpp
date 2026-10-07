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

#ifndef GENMC_THREAD_PINNER_HPP
#define GENMC_THREAD_PINNER_HPP

#include <memory>

/*******************************************************************************
 **                           ThreadPinner Class
 ******************************************************************************/

/**
 * A class responsible for pinning the workers of a thread pool to CPUs.
 *
 * Pinning is opt-in and best effort: it only happens if requested, if GenMC
 * was built with hwloc, and if the platform can bind threads. Workers are
 * spread over the CPUs the constructing thread may run on, so restrictions
 * like taskset(1) are honored. hwloc stays out of this header, so the layout
 * of the class does not depend on how GenMC was built.
 */
class ThreadPinner {

public:
	/*** Constructors ***/

	/** Reserves a CPU for each of `numThreads` workers, if `enabled` */
	ThreadPinner(unsigned int numThreads, bool enabled);
	ThreadPinner() = delete;
	ThreadPinner(const ThreadPinner &) = delete;
	ThreadPinner(ThreadPinner &&) = delete;

	auto operator=(const ThreadPinner &) -> ThreadPinner & = delete;
	auto operator=(ThreadPinner &&) -> ThreadPinner & = delete;

	/*** Destructor ***/

	~ThreadPinner();

	/** Returns whether workers will actually be pinned */
	[[nodiscard]] auto isEnabled() const -> bool { return impl_ != nullptr; }

	/** Pins the calling thread to the CPU reserved for worker `index`.
	 * Workers must pin themselves: pinning a thread from outside races with
	 * its exit. Workers can call this concurrently. */
	void pinCurrentThread(unsigned int index) const;

private:
	struct Impl;

	/** The CPU reserved for each worker (null if pinning is disabled) */
	std::unique_ptr<Impl> impl_;
};

#endif /* GENMC_THREAD_PINNER_HPP */
