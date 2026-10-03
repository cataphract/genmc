#include <stdatomic.h>

atomic_int y;
atomic_int z = 2;

/* Annotating the CAS has to get past a switch on its way to the latch. */
int main(void)
{
	for (;;) {
		int e = 0;
		if (atomic_compare_exchange_strong(&y, &e, 1))
			break;
		switch (atomic_load(&z)) {
		case 0:
			(void)atomic_load(&y);
			break;
		case 1:
			(void)atomic_load(&z);
			break;
		default:
			break;
		}
	}
	return 0;
}
