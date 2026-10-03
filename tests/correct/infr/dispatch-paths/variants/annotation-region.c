#include <genmc.h>
#include <stdatomic.h>

/* The number of dispatches (at most 64) */
#ifndef N
#define N 40
#endif

atomic_int x;
int sel[64];

static int f0(int v) { return v + 1; }
static int f1(int v) { return v + 2; }

static int (*const table[2])(int) = {f0, f1};

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

/* Finding the atomic instructions of the annotated region must not walk
 * each of the paths through its dispatches. */
int main(void)
{
	int v = 0;
	__VERIFIER_final_write(STEP64(0); atomic_store_explicit(&x, v, memory_order_relaxed));
	return 0;
}
