#include <genmc.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int x;
atomic_int selector;

static int f0(void) { return 0; }
static int f1(void) { return 1; }

static int (*const table[2])(void) = {f0, f1};

void *thread(void *arg)
{
	atomic_store_explicit(&x, 1, memory_order_relaxed);
	return NULL;
}

/* The table is read before x, so only the promoted dispatch lies between the
 * load of x and the assume that annotates it. */
int main(void)
{
	pthread_t t;
	pthread_create(&t, NULL, thread, NULL);
	int (*f)(void) = table[atomic_load_explicit(&selector, memory_order_relaxed) & 1];
	int v = atomic_load_explicit(&x, memory_order_relaxed);
	__VERIFIER_assume(f() + v == 1);
	pthread_join(t, NULL);
	return 0;
}
