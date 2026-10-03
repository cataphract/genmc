#include <stdatomic.h>

atomic_int x;
atomic_int a = 1;

/* The decrement sits in an inner loop body and runs twice per outer
 * iteration, so blocking before it as a zero-net-effect iteration would stop
 * the loop before its second iteration exits. */
int main(void)
{
	for (;;) {
		if (atomic_fetch_add_explicit(&x, 1, memory_order_relaxed) != 0)
			break;
		int k = 0;
		while (atomic_load_explicit(&a, memory_order_relaxed) && k < 2) {
			atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
			k++;
		}
	}
	return 0;
}
