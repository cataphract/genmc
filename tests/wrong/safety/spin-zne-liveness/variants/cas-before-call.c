#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int x, y, z, done;

/* With liveness checks, this loop gets a spin_start */
void wait_z(void)
{
	while (atomic_load(&z) == 5)
		;
}

/* Like cas-before-spin-wait.c, but the inner spinloop runs in a call (which
 * is not inlined) */
void *t1(void *arg)
{
	for (;;) {
		atomic_fetch_add(&x, 1);
		int e = 0;
		atomic_compare_exchange_strong(&y, &e, 1);
		wait_z();
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

int main(void)
{
	pthread_t p, q;
	pthread_create(&p, NULL, t1, NULL);
	pthread_create(&q, NULL, t2, NULL);
	return 0;
}
