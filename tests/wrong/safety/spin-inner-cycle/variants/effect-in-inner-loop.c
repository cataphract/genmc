#include <assert.h>
#include <stdatomic.h>

atomic_int x;

/* The outer loop's only effect lives in an inner loop body, which is not on
 * any simple path from the outer header to the outer latch. Treating the
 * outer loop as effect-free would block the second iteration, hiding the
 * assertion failure below. */
int main(void)
{
	while (atomic_load_explicit(&x, memory_order_relaxed) < 2) {
		for (int i = 0; i < 1; i++)
			atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
	}
	assert(atomic_load_explicit(&x, memory_order_relaxed) != 2);
	return 0;
}
