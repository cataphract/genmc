#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

pthread_mutex_t m;
atomic_int a = 1;
atomic_int done;

/* Same as lock-inner.c but with a do-while, whose body is on a simple
 * header-to-latch path. */
int main(void)
{
	for (;;) {
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		int k = 0;
		do {
			pthread_mutex_lock(&m);
			pthread_mutex_unlock(&m);
			k++;
		} while (atomic_load_explicit(&a, memory_order_relaxed) && k < 2);
		assert(k < 2);
	}
	return 0;
}
