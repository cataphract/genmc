#include <stdatomic.h>

atomic_int y;
atomic_int g;
_Atomic(atomic_int *) p;

/* Whether a failed CAS leads to the header depends on the address of a
 * global, which is only known at runtime. */
int main(void)
{
	for (;;) {
		int e = 0;
		if (atomic_compare_exchange_strong(&y, &e, 1))
			break;
		if (atomic_load(&p) == &g)
			break;
	}
	return 0;
}
