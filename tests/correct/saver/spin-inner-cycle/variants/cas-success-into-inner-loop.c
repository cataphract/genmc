#include <stdatomic.h>

atomic_int w;

/* A successful CAS returns to the header through an inner loop, so the
 * iteration that wrote w is not effect-free. */
int main(void)
{
	for (;;) {
		int e = 0;
		if (!atomic_compare_exchange_strong(&w, &e, 1))
			break;
		for (int i = 0; i < 1; i++)
			(void)atomic_load_explicit(&w, memory_order_relaxed);
	}
	return 0;
}
