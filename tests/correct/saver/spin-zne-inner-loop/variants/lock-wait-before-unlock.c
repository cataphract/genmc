#include <pthread.h>
#include <stdatomic.h>

pthread_mutex_t m;
atomic_int y, done;

/* The effect-free inner loop runs between the lock and the unlock, which run
 * once per iteration, so every iteration still blocks before the unlock. */
int main(void)
{
	for (;;) {
		pthread_mutex_lock(&m);
		for (int i = 0; i < 2; i++)
			(void)atomic_load_explicit(&y, memory_order_relaxed);
		if (atomic_load_explicit(&done, memory_order_relaxed))
			break;
		pthread_mutex_unlock(&m);
	}
	pthread_mutex_unlock(&m);
	return 0;
}
