#include <stdatomic.h>

atomic_int y;
atomic_int sel;

/* Each handler succeeds its CAS, as the program has a single thread */
#define HANDLER(i)                                                                                 \
	static int handler##i(void)                                                                \
	{                                                                                          \
		int e = atomic_load_explicit(&y, memory_order_relaxed);                            \
		return atomic_compare_exchange_strong(&y, &e, e + 1);                              \
	}
#define EACH15(F) F(0) F(1) F(2) F(3) F(4) F(5) F(6) F(7) F(8) F(9) F(10) F(11) F(12) F(13) F(14)
#define LIST15(f)                                                                                  \
	f##0, f##1, f##2, f##3, f##4, f##5, f##6, f##7, f##8, f##9, f##10, f##11, f##12, f##13,    \
		f##14

EACH15(HANDLER)
HANDLER(15)

static int (*const first[16])(void) = {LIST15(handler), handler15};
static int (*const second[15])(void) = {LIST15(handler)};

/* Inlining the promoted dispatches leaves 31 CASes in the loop. A successful
 * one returns to the header, so the loop is no spinloop, and must run twice.
 * Checking each combination of CAS results must neither take 2^31 steps, nor
 * overflow counting them, which skipped the check. */
int main(void)
{
	for (;;) {
		first[atomic_load_explicit(&sel, memory_order_relaxed) & 15]();
		second[atomic_load_explicit(&sel, memory_order_relaxed) % 15]();
		if (atomic_load_explicit(&y, memory_order_relaxed) >= 4)
			break;
	}
	return 0;
}
