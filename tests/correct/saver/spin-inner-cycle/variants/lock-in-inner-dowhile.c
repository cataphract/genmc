#include <pthread.h>
#include <stdatomic.h>

pthread_mutex_t m;
atomic_int a = 1;

/* Same as lock-in-inner-loop.c, but the inner loop is a do-while, whose body
 * lies on a simple path from the outer header to the outer latch. */
int main(void)
{
	for (;;) {
		int k = 0;
		do {
			pthread_mutex_lock(&m);
			if (k++ == 1)
				goto out;
			pthread_mutex_unlock(&m);
		} while (atomic_load_explicit(&a, memory_order_relaxed));
	}
out:
	pthread_mutex_unlock(&m);
	return 0;
}
