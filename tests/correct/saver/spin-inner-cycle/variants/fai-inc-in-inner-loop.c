#include <stdatomic.h>

atomic_int x;
atomic_int a = 1;

/* The increment repeats in an inner do-while (on a simple path), while the
 * decrement runs once, so an iteration has a net effect. */
int main(void)
{
	for (;;) {
		int k = 0;
		int v;
		do {
			v = atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
			k++;
		} while (atomic_load_explicit(&a, memory_order_relaxed) && k < 2);
		if (v != 1)
			break;
		atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
	}
	return 0;
}
