#include <stdatomic.h>

atomic_int g;
_Atomic(atomic_int *) p = &g;

/* Annotating the load of the spinloop has to compare with the address of a
 * global, which is only known at runtime. */
int main(void)
{
	while (atomic_load(&p) != &g)
		;
	return 0;
}
