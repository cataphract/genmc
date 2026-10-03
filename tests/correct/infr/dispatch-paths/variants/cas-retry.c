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

/* A failed CAS runs the dispatches before the loop retries. Checking where a
 * successful CAS leads must not build a condition with one case per path
 * from the CAS back to the header. */
int main(void)
{
	for (;;) {
		int expected = 0;
		if (atomic_compare_exchange_strong(&x, &expected, 1))
			break;
		int v = 0;
		STEP64(0);
		if (v < 0)
			break;
	}
	return 0;
}
