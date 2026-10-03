#include <stdatomic.h>

atomic_int y;
atomic_int z;

/* Annotating the CAS has to get past a store to local memory, which the
 * effect check allows. */
int main(void)
{
	int buf[4];
	for (;;) {
		int e = 0;
		if (atomic_compare_exchange_strong(&y, &e, 1))
			break;
		buf[atomic_load(&z) & 3] = 1;
	}
	return 0;
}
