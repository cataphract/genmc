#include <stdatomic.h>

atomic_int x, y, w, done;

/* Not inlined, as it is recursive. Its loop gets a spin_start with liveness
 * checks. */
void wait_y(int n)
{
	while (atomic_load_explicit(&y, memory_order_relaxed))
		;
	if (n > 0)
		wait_y(n - 1);
}

/* Like fai-spin-wait-before-cas.c, but the spinloop runs in a call */
int main(void)
{
	for (;;) {
		atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
		wait_y(0);
		int e = 1;
		atomic_compare_exchange_strong(&w, &e, 2);
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
	}
	return 0;
}
