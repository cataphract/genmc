#include <stdatomic.h>
#include <stdint.h>

atomic_int y;
atomic_int g;
_Atomic(intptr_t) q;

/* Like cas-ptr-cmp.c, but the address is an integer constant expression. */
int main(void)
{
	for (;;) {
		int e = 0;
		if (atomic_compare_exchange_strong(&y, &e, 1))
			break;
		if (atomic_load(&q) == (intptr_t)&g)
			break;
	}
	return 0;
}
