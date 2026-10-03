#include <stdatomic.h>

atomic_int y;
atomic_int z;

/* Annotating the CAS has to get past a phi between floating-point constants,
 * which annotations do not represent. */
int main(void)
{
	for (;;) {
		int e = 0;
		if (atomic_compare_exchange_strong(&y, &e, 1))
			break;
		float f = atomic_load(&z) ? 1.5f : 2.5f;
		if (f > 2.0f)
			break;
	}
	return 0;
}
