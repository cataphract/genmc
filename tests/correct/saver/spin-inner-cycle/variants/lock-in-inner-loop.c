#include <pthread.h>
#include <stdatomic.h>

pthread_mutex_t m;
atomic_int a = 1;

/* The only lock/unlock pair of the outer loop sits in an inner while body,
 * so an outer iteration can run it twice. */
int main(void)
{
	for (;;) {
		int k = 0;
		while (atomic_load_explicit(&a, memory_order_relaxed)) {
			pthread_mutex_lock(&m);
			if (k++ == 1)
				goto out;
			pthread_mutex_unlock(&m);
		}
	}
out:
	pthread_mutex_unlock(&m);
	return 0;
}
