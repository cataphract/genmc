#include <stdatomic.h>

atomic_int x, y, w, done;

/* Like fai-spin-wait-before-dec.c, but a (failing) CAS runs after the inner
 * spinloop, so the runtime check still sees whether it succeeded. */
int main(void)
{
	for (;;) {
		atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
		while (atomic_load_explicit(&y, memory_order_relaxed))
			;
		int e = 1;
		atomic_compare_exchange_strong(&w, &e, 2);
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
	}
	return 0;
}
