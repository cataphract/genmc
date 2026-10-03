#include <pthread.h>
#include <stdatomic.h>

atomic_int x;

static int f0(void) { return 0; }
static int f1(void) { return 1; }

static int (*const table[2])(void) = {f0, f1};

void *thread(void *arg)
{
	atomic_store_explicit(&x, 1, memory_order_release);
	return NULL;
}

/* The spinloop's condition goes through the promoted dispatch, which the
 * annotation of its load has to handle. */
int main(void)
{
	pthread_t t;
	pthread_create(&t, NULL, thread, NULL);
	while (table[atomic_load_explicit(&x, memory_order_acquire) & 1]() == 0)
		;
	pthread_join(t, NULL);
	return 0;
}
