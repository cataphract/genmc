#include <stdatomic.h>

atomic_int y;
atomic_int z;

/* Like cas-local-store.c, but the store is in an inner loop body. */
int main(void)
{
	int buf[4];
	for (;;) {
		int e = 0;
		if (atomic_compare_exchange_strong(&y, &e, 1))
			break;
		for (int i = 0; i < 2; i++)
			buf[(atomic_load(&z) + i) & 3] = 1;
	}
	return 0;
}
