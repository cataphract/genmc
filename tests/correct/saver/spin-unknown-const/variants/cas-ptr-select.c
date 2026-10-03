#include <stdatomic.h>

atomic_int y;
atomic_int z;
atomic_int g1, g2;

/* Annotating the CAS has to get past a select between the addresses of two
 * globals, which are only known at runtime. */
int main(void)
{
	for (;;) {
		int e = 0;
		if (atomic_compare_exchange_strong(&y, &e, 1))
			break;
		atomic_int *p = atomic_load(&z) ? &g1 : &g2;
		(void)atomic_load(p);
	}
	return 0;
}
