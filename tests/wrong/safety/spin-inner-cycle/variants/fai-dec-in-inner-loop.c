#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int x;
atomic_int a;
atomic_int done;

/* The decrement sits in an inner while body, on no simple path from the
 * outer header to the outer latch. Once t2 sets a, the inner loop decrements
 * twice in one outer iteration, so blocking before the first decrement as a
 * zero-net-effect iteration would hide the failure below. */
void *t1(void *arg)
{
	for (;;) {
		atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		int k = 0;
		while (atomic_load_explicit(&a, memory_order_relaxed) && k < 2) {
			atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
			k++;
		}
		assert(k < 2);
	}
	return NULL;
}

void *t2(void *arg)
{
	atomic_store_explicit(&a, 1, memory_order_relaxed);
	atomic_store_explicit(&done, 1, memory_order_relaxed);
	return NULL;
}

int main(void)
{
	pthread_t p, q;
	pthread_create(&p, NULL, t1, NULL);
	pthread_create(&q, NULL, t2, NULL);
	pthread_join(p, NULL);
	pthread_join(q, NULL);
	return 0;
}
