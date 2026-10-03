#include <stdatomic.h>

atomic_int x;
atomic_int y;

/* The inner do-while has its own spin_start. Its CAS succeeds, and then the
 * inner loop runs once more, so no write follows the latest spin_start when
 * the decrement is reached, although this outer iteration wrote y. */
int main(void)
{
	for (;;) {
		if (atomic_load(&y))
			break;
		atomic_fetch_add(&x, 1);
		int r;
		do {
			int e = 0;
			r = atomic_compare_exchange_strong(&y, &e, 1);
		} while (r);
		atomic_fetch_sub(&x, 1);
	}
	return 0;
}
