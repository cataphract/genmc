#include <stdatomic.h>

atomic_int x;

/* The outer loop's only effect is in an inner loop body, which lies on no
 * simple path from the outer header to the outer latch. The outer loop must
 * not be treated as an effect-free spinloop. */
int main(void)
{
	while (atomic_load_explicit(&x, memory_order_relaxed) < 2)
		for (int i = 0; i < 1; i++)
			atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
	return 0;
}
