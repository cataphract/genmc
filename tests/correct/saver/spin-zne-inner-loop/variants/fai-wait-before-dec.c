#include <stdatomic.h>

atomic_int x, y, done;

/* The effect-free inner loop runs before the decrement but has no spin_start
 * of its own, and neither FAI repeats, so every iteration still has zero net
 * effect and blocks before the decrement. */
int main(void)
{
	for (;;) {
		atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
		for (int i = 0; i < 2; i++)
			(void)atomic_load_explicit(&y, memory_order_relaxed);
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
	}
	return 0;
}
