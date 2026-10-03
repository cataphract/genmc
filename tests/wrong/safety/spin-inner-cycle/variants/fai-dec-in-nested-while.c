#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int x, stop;
atomic_int y = 1;

/* While y is set, the inner loop decrements x more than once per increment,
 * so x can reach -1. Blocking before the first decrement would hide it. */
void *t1(void *arg)
{
	while (!atomic_load(&stop)) {
		atomic_fetch_add(&x, 1);
		while (atomic_load(&y))
			atomic_fetch_sub(&x, 1);
	}
	return NULL;
}

void *t2(void *arg)
{
	int r = atomic_fetch_add(&x, 0);
	assert(r != -1);
	return NULL;
}

void *t3(void *arg)
{
	atomic_store(&y, 0);
	atomic_store(&stop, 1);
	return NULL;
}

int main()
{
	pthread_t a, b, c;
	pthread_create(&a, NULL, t1, NULL);
	pthread_create(&b, NULL, t2, NULL);
	pthread_create(&c, NULL, t3, NULL);
	return 0;
}
