#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int x, z, done;
int y;

/* With liveness checks, this loop gets a spin_start */
void wait_z(void)
{
	while (atomic_load(&z) == 5)
		;
}

/* Like cas-before-call.c, but the CAS (unlike a C11 one) does not end its
 * block, so the call follows it in the same block */
void *t1(void *arg)
{
	for (;;) {
		atomic_fetch_add(&x, 1);
		__sync_val_compare_and_swap(&y, 0, 1);
		wait_z();
		if (atomic_load(&done))
			break;
		atomic_fetch_sub(&x, 1);
	}
	return NULL;
}

void *t2(void *arg)
{
	if (__atomic_load_n(&y, __ATOMIC_SEQ_CST) == 1)
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
