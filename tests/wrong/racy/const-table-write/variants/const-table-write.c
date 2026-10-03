#include <assert.h>
#include <pthread.h>
#include <stddef.h>

static int first(void) { return 1; }
static int second(void) { return 2; }

static int (*const table[2])(void) = {first, second};
static volatile unsigned long index = 0;

void *thread(void *arg)
{
	/* Undefined behavior: writes to a const object */
	*(int (**)(void)) & table[0] = NULL;
	return NULL;
}

/* Promoting the dispatch must keep the table read, which races with the
 * write above. */
int main(void)
{
	pthread_t t;
	pthread_create(&t, NULL, thread, NULL);
	assert(table[index]() == 1);
	pthread_join(t, NULL);
	return 0;
}
