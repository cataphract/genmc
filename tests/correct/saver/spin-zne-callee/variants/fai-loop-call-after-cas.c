#include <stdatomic.h>

atomic_int x, y, w, done;

/* Not inlined, as it is recursive. Without liveness checks, its loop has no
 * side effects and thus no spin_start. */
void wait_y(int n)
{
	while (atomic_load_explicit(&y, memory_order_relaxed))
		;
	if (n > 0)
		wait_y(n - 1);
}

/* The call follows the (failing) CAS, but its loop has no spin_start that
 * could hide the write of the CAS */
int main(void)
{
	for (;;) {
		atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
		int e = 1;
		atomic_compare_exchange_strong(&w, &e, 2);
		wait_y(0);
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
	}
	return 0;
}
