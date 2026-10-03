#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

pthread_mutex_t m;
atomic_int a = 1;
atomic_int done;

/* The lock/unlock pair sits in an inner while-body, so it can run twice in a
 * single outer iteration. Blocking at its first unlock would hide the
 * failure below. */
int main(void)
{
	for (;;) {
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		int k = 0;
		while (atomic_load_explicit(&a, memory_order_relaxed) && k < 2) {
			pthread_mutex_lock(&m);
			pthread_mutex_unlock(&m);
			k++;
		}
		assert(k < 2);
	}
	return 0;
}
