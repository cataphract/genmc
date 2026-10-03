#include <stdatomic.h>

atomic_int x, y, w, done;

/* Not inlined, as it is recursive. It runs no loop, so no spin_start. */
int peek_y(int n) { return n > 0 ? peek_y(n - 1) : atomic_load(&y); }

/* A call follows the (failing) CAS, but cannot hide its write */
int main(void)
{
	for (;;) {
		atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
		int e = 1;
		atomic_compare_exchange_strong(&w, &e, 2);
		(void)peek_y(1);
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
	}
	return 0;
}
