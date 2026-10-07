#include <gtest/gtest.h>

#include "genmc/Support/ThreadPinner.hpp"

#include <thread>
#include <type_traits>
#include <vector>

#ifdef __linux__
#include <cstdlib>
#include <sched.h>
#endif

/* A copy would share the placement, and free it twice */
static_assert(!std::is_copy_constructible_v<ThreadPinner>);
static_assert(!std::is_copy_assignable_v<ThreadPinner>);
static_assert(!std::is_move_constructible_v<ThreadPinner>);
static_assert(!std::is_move_assignable_v<ThreadPinner>);

TEST(ThreadPinnerTest, DisabledUnlessRequested)
{
	const ThreadPinner pinner(4, false);
	EXPECT_FALSE(pinner.isEnabled());
	pinner.pinCurrentThread(0); /* no-op */
}

TEST(ThreadPinnerTest, NoWorkers)
{
	/* Older hwloc crashes when asked to distribute nothing */
	const ThreadPinner pinner(0, true);
	EXPECT_FALSE(pinner.isEnabled());
	pinner.pinCurrentThread(0); /* no-op */
}

#ifdef __linux__

namespace {

auto getAffinity() -> cpu_set_t
{
	cpu_set_t set;
	CPU_ZERO(&set);
	EXPECT_EQ(sched_getaffinity(0, sizeof(set), &set), 0);
	return set;
}

/* Restores the calling thread's affinity on scope exit */
class AffinityGuard {
public:
	AffinityGuard() : saved_(getAffinity()) {}
	AffinityGuard(const AffinityGuard &) = delete;
	AffinityGuard(AffinityGuard &&) = delete;
	auto operator=(const AffinityGuard &) -> AffinityGuard & = delete;
	auto operator=(AffinityGuard &&) -> AffinityGuard & = delete;
	~AffinityGuard() { sched_setaffinity(0, sizeof(saved_), &saved_); }

	[[nodiscard]] auto getSaved() const -> const cpu_set_t & { return saved_; }

private:
	cpu_set_t saved_;
};

/* Returns the affinity of each of `n` workers after it pinned itself */
auto pinWorkers(const ThreadPinner &pinner, unsigned int n) -> std::vector<cpu_set_t>
{
	std::vector<cpu_set_t> sets(n);
	std::vector<std::thread> workers;
	for (auto i = 0U; i < n; i++) {
		workers.emplace_back([&pinner, &sets, i] {
			pinner.pinCurrentThread(i);
			sets[i] = getAffinity();
		});
	}
	for (auto &w : workers)
		w.join();
	return sets;
}

} // namespace

TEST(ThreadPinnerTest, DisabledPinnerKeepsAffinity)
{
	const auto inherited = getAffinity();
	const ThreadPinner pinner(4, false);
	for (const auto &set : pinWorkers(pinner, 4))
		EXPECT_TRUE(CPU_EQUAL(&set, &inherited));
}

TEST(ThreadPinnerTest, PinsWithinInheritedAffinity)
{
	const AffinityGuard guard;

	/* Like taskset(1): confine this thread to (at most) two of its CPUs.
	 * Its threads inherit that, but are free to bind themselves elsewhere */
	cpu_set_t allowed;
	CPU_ZERO(&allowed);
	for (auto cpu = 0, kept = 0; cpu < CPU_SETSIZE && kept < 2; cpu++) {
		if (CPU_ISSET(cpu, &guard.getSaved())) {
			CPU_SET(cpu, &allowed);
			kept++;
		}
	}
	if (sched_setaffinity(0, sizeof(allowed), &allowed) != 0)
		GTEST_SKIP() << "cannot change the affinity of this thread";

	const ThreadPinner pinner(4, true);
	if (!pinner.isEnabled())
		GTEST_SKIP() << "thread pinning is unavailable in this build or environment";

	cpu_set_t used;
	CPU_ZERO(&used);
	for (const auto &set : pinWorkers(pinner, 4)) {
		EXPECT_EQ(CPU_COUNT(&set), 1);
		cpu_set_t inside;
		CPU_AND(&inside, &set, &allowed);
		EXPECT_TRUE(CPU_EQUAL(&inside, &set)) << "worker pinned outside the allowed CPUs";
		CPU_OR(&used, &used, &set);
	}
	EXPECT_TRUE(CPU_EQUAL(&used, &allowed)) << "workers not spread over the allowed CPUs";

	/* Workers pin themselves, never the thread that spawned them */
	const auto after = getAffinity();
	EXPECT_TRUE(CPU_EQUAL(&after, &allowed));
}

TEST(ThreadPinnerTest, UnsupportedBindingDisablesPinning)
{
	/* hwloc cannot bind threads for the topology of another system (nor at all
	 * on macOS): the pinner must notice, rather than "pin" without effect */
	setenv("HWLOC_THISSYSTEM", "0", 1);
	const ThreadPinner pinner(4, true);
	unsetenv("HWLOC_THISSYSTEM");
	EXPECT_FALSE(pinner.isEnabled());
}

#endif /* __linux__ */
