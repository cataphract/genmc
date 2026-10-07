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

#include "genmc/Support/ThreadPinner.hpp"
#include "genmc/Support/Error.hpp"

#ifdef HAVE_LIBHWLOC

#include <hwloc.h>
#include <hwloc/bitmap.h>

#include <atomic>
#include <cerrno>
#include <limits>
#include <memory>
#include <system_error>
#include <vector>

/** The CPU reserved for each worker, and the topology needed to bind to it */
struct ThreadPinner::Impl {
	Impl() = default;
	Impl(const Impl &) = delete;
	Impl(Impl &&) = delete;
	auto operator=(const Impl &) -> Impl & = delete;
	auto operator=(Impl &&) -> Impl & = delete;
	~Impl();

	/** Returns a placement for `numThreads` workers,
	 * or null (after warning) if they cannot be pinned */
	static auto create(unsigned int numThreads) -> std::unique_ptr<Impl>;

	hwloc_topology_t topology{};
	std::vector<hwloc_cpuset_t> cpusets;

	/** Whether some worker already reported failing to pin itself */
	std::atomic_flag reportedFailure;
};

static auto restrictToCallerBinding(hwloc_topology_t topology) -> bool;

ThreadPinner::ThreadPinner(unsigned int numThreads, bool enabled)
{
	if (enabled && numThreads > 0)
		impl_ = Impl::create(numThreads);
}

void ThreadPinner::pinCurrentThread(unsigned int index) const
{
	if (!impl_ || index >= impl_->cpusets.size())
		return;

	if (hwloc_set_cpubind(impl_->topology, impl_->cpusets[index], HWLOC_CPUBIND_THREAD) < 0) {
		const auto err = errno;
		if (!impl_->reportedFailure.test_and_set())
			WARN("Could not pin worker threads to CPUs: {}",
			     std::generic_category().message(err));
	}
}

auto ThreadPinner::Impl::create(unsigned int numThreads) -> std::unique_ptr<Impl>
{
	auto impl = std::make_unique<Impl>();
	if (hwloc_topology_init(&impl->topology) < 0 || hwloc_topology_load(impl->topology) < 0) {
		WARN("Could not discover the CPU topology; threads will not be pinned");
		return nullptr;
	}

	/* E.g., macOS cannot bind threads at all */
	if (!hwloc_topology_get_support(impl->topology)->cpubind->set_thisthread_cpubind) {
		WARN("Thread pinning is not supported on this platform");
		return nullptr;
	}

	if (!restrictToCallerBinding(impl->topology)) {
		WARN("Could not determine which CPUs threads may use; threads will not be pinned");
		return nullptr;
	}

	impl->cpusets.resize(numThreads);
	auto *root = hwloc_get_root_obj(impl->topology);
	if (hwloc_distrib(impl->topology, &root, 1, impl->cpusets.data(), numThreads,
			  std::numeric_limits<int>::max(), 0) < 0) {
		WARN("Could not distribute threads over CPUs; threads will not be pinned");
		return nullptr;
	}

	/* Minimize migration costs */
	for (auto *set : impl->cpusets)
		hwloc_bitmap_singlify(set);
	return impl;
}

ThreadPinner::Impl::~Impl()
{
	for (auto *set : cpusets)
		hwloc_bitmap_free(set);
	if (topology != nullptr)
		hwloc_topology_destroy(topology);
}

/* Restricts the topology to the CPUs the calling thread may run on. The topology
 * already lacks the CPUs that cgroups exclude, but not those that
 * sched_setaffinity(2) excludes (e.g., via taskset(1)), which threads may undo.
 * Returns whether the topology can still be used. */
static auto restrictToCallerBinding(hwloc_topology_t topology) -> bool
{
	/* Without a way to tell, assume the whole topology is available */
	if (!hwloc_topology_get_support(topology)->cpubind->get_thisthread_cpubind)
		return true;

	auto *binding = hwloc_bitmap_alloc();
	if (binding == nullptr)
		return false;

	auto usable = hwloc_get_cpubind(topology, binding, HWLOC_CPUBIND_THREAD) == 0;
	if (usable && !hwloc_bitmap_isincluded(hwloc_get_root_obj(topology)->cpuset, binding))
		usable = hwloc_topology_restrict(topology, binding, 0) == 0;
	hwloc_bitmap_free(binding);
	return usable;
}

#else /* !HAVE_LIBHWLOC */

struct ThreadPinner::Impl {};

ThreadPinner::ThreadPinner(unsigned int numThreads, bool enabled)
{
	if (enabled && numThreads > 0)
		WARN("Thread pinning is unavailable: GenMC was built without hwloc");
}

void ThreadPinner::pinCurrentThread(unsigned int /*index*/) const {}

#endif /* HAVE_LIBHWLOC */

ThreadPinner::~ThreadPinner() = default;
