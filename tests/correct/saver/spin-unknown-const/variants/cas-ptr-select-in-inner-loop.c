#include <stdatomic.h>

atomic_int y;
atomic_int z;
atomic_int g1, g2;

/* Like cas-ptr-select.c, but the select is in an inner loop body. */
int main(void)
{
	for (;;) {
		int e = 0;
		if (atomic_compare_exchange_strong(&y, &e, 1))
			break;
		for (int i = 0; i < 2; i++) {
			atomic_int *p = atomic_load(&z) ? &g1 : &g2;
			(void)atomic_load(p);
		}
	}
	return 0;
}
