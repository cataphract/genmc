#include <assert.h>
#include <stdatomic.h>

atomic_int w;
atomic_int done;

/* The first iteration's CAS succeeds and then runs an effect-free inner
 * loop on its way back to the header, so that iteration has an effect. */
void *t1(void *arg)
{
	for (;;) {
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		int e = 0;
		if (atomic_compare_exchange_strong(&w, &e, 1))
			for (int i = 0; i < 1; i++)
				(void)atomic_load_explicit(&w, memory_order_relaxed);
	}
	assert(atomic_load_explicit(&w, memory_order_relaxed) == 0);
	return NULL;
}

void *t2(void *arg)
{
	atomic_store_explicit(&done, 1, memory_order_relaxed);
	return NULL;
}

#include <pthread.h>
int main(void)
{
	pthread_t p, q;
	pthread_create(&p, NULL, t1, NULL);
	pthread_create(&q, NULL, t2, NULL);
	return 0;
}
