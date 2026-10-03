#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int x;
atomic_int a = 1;
atomic_int done;

/* The increment repeats in an inner do-while, while the decrement runs once,
 * so each outer iteration adds one to x. Blocking before the first decrement
 * would keep x below 3. */
void *t1(void *arg)
{
	for (;;) {
		int k = 0;
		do {
			atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
			k++;
		} while (atomic_load_explicit(&a, memory_order_relaxed) && k < 2);
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed);
	}
	return NULL;
}

void *t2(void *arg)
{
	assert(atomic_fetch_add_explicit(&x, 0, memory_order_relaxed) != 3);
	return NULL;
}

int main(void)
{
	pthread_t p, q;
	pthread_create(&p, NULL, t1, NULL);
	pthread_create(&q, NULL, t2, NULL);
	return 0;
}
