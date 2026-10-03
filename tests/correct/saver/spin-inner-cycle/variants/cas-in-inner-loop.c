#include <stdatomic.h>

atomic_int y;
atomic_int w = 1;
atomic_int z;

/* The first iteration's CAS on y succeeds; the second one fails and exits.
 * The CAS on z lies in an inner loop body, on no simple path from the outer
 * header to the outer latch. */
int main(void)
{
	for (;;) {
		int e = 0;
		if (!atomic_compare_exchange_strong(&y, &e, 1))
			break;
		int e3 = 0;
		for (int k = 0; k < 2 && atomic_compare_exchange_strong(&w, &e3, 1); k++) {
			int e2 = 0;
			if (atomic_compare_exchange_strong(&z, &e2, 1))
				(void)atomic_load(&w);
			e3 = 0;
		}
	}
	return 0;
}
