#include <stdatomic.h>

/* The number of dispatches (at most 64) */
#ifndef N
#define N 40
#endif

atomic_int x;
atomic_int y;
int sel[64];

/* Handlers that keep the value: once inlined, each dispatch merges it with Φs */
static int keep(int v) { return v; }
static int keepEither(int v)
{
	if (sel[63])
		return v;
	return v;
}

static int (*const table[2])(int) = {keep, keepEither};

/* Promoting a dispatch through the constant table, and inlining its targets,
 * turns it into a diamond. N of them in a row give 2^N paths. */
#define STEP(i)                                                                                    \
	if ((i) < N)                                                                               \
	v = table[sel[i] & 1](v)
#define STEP4(i)                                                                                   \
	STEP(i);                                                                                   \
	STEP((i) + 1);                                                                             \
	STEP((i) + 2);                                                                             \
	STEP((i) + 3)
#define STEP16(i)                                                                                  \
	STEP4(i);                                                                                  \
	STEP4((i) + 4);                                                                            \
	STEP4((i) + 8);                                                                            \
	STEP4((i) + 12)
#define STEP64(i)                                                                                  \
	STEP16(i);                                                                                 \
	STEP16((i) + 16);                                                                          \
	STEP16((i) + 32);                                                                          \
	STEP16((i) + 48)

/* The loop carries the value of a load through the dispatches. Checking that
 * the Φ of the loop header only merges values of that load must not follow
 * each of the ways the Φs of the dispatches merge it. */
int main(void)
{
	int v = atomic_load_explicit(&y, memory_order_relaxed);
	for (;;) {
		STEP64(0);
		if (atomic_load_explicit(&x, memory_order_relaxed) == 0)
			break;
	}
	return v;
}
