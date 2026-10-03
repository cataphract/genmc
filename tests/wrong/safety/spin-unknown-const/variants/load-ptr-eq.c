#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int g, h;
_Atomic(atomic_int *) p = &g;

/* The spinloop exits once P stops pointing to G. The annotation of its load
 * cannot know the address of G, and must not take it to be the value read:
 * then every read would seem to fail the loop exit, and get blocked. */
void *t1(void *arg)
{
	while (atomic_load(&p) == &g)
		;
	assert(0);
	return NULL;
}

void *t2(void *arg)
{
	atomic_store(&p, &h);
	return NULL;
}

int main(void)
{
	pthread_t a, b;
	pthread_create(&a, NULL, t1, NULL);
	pthread_create(&b, NULL, t2, NULL);
	return 0;
}
