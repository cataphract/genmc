#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

/* The first execution leaves winner == 2. Revisited executions can fail the
 * assertion in several pool workers while further tasks remain queued. Once
 * one worker reports the error, workers with interrupted metadata replays must
 * stop instead of starting a queued graph with the old replay event. */
static atomic_int count;
static atomic_int winner;

static void *worker(void *arg);

int main(void)
{
	pthread_t first, second;
	pthread_create(&first, NULL, worker, (void *)1L);
	pthread_create(&second, NULL, worker, (void *)2L);
	pthread_join(first, NULL);
	pthread_join(second, NULL);
	assert(atomic_load_explicit(&winner, memory_order_relaxed) != 1);
	return 0;
}

static void *worker(void *arg)
{
	for (int i = 0; i < 4; ++i)
		atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
	atomic_store_explicit(&winner, (int)(long)arg, memory_order_relaxed);
	return NULL;
}
