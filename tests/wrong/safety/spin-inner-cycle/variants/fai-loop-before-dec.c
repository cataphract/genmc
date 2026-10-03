#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int x, y, done;
atomic_int z = 1;

/* A successful CAS always makes the inner do-while run once more, so the
 * inner loop's spin_start comes after the CAS write. The outer iteration then
 * has an effect even though no write follows the latest spin_start. */
void *t1(void *arg)
{
	for (;;) {
		atomic_fetch_add(&x, 1);
		int r;
		do {
			int e = 0;
			r = atomic_compare_exchange_strong(&y, &e, 1);
		} while (atomic_load(&z) || r);
		if (atomic_load(&done))
			break;
		atomic_fetch_sub(&x, 1);
	}
	return NULL;
}

void *t2(void *arg)
{
	if (atomic_load(&y) == 1)
		assert(atomic_fetch_add(&x, 0) != 0);
	return NULL;
}

void *t3(void *arg)
{
	atomic_store(&z, 0);
	return NULL;
}

int main(void)
{
	pthread_t p, q, r;
	pthread_create(&p, NULL, t1, NULL);
	pthread_create(&q, NULL, t2, NULL);
	pthread_create(&r, NULL, t3, NULL);
	return 0;
}
