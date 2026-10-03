#include <stdatomic.h>

atomic_int x, y, done;

/* With liveness checks, the inner spinloop gets a spin_start of its own. No
 * CAS runs before it, so it hides no write, and every iteration still has
 * zero net effect and blocks before the decrement. */
int main(void)
{
	for (;;) {
		atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
		while (atomic_load_explicit(&y, memory_order_relaxed))
			;
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
	}
	return 0;
}
