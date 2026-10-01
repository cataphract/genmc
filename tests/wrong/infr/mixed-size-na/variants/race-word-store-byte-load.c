#include <pthread.h>
#include <stdint.h>

uint32_t word;

void *writer(void *unused)
{
	word = UINT32_C(0x11111111);
	return NULL;
}

int main(void)
{
	pthread_t thread;
	pthread_create(&thread, NULL, writer, NULL);
	/* The race is a program error and must not be reported as a tool limitation. */
	unsigned char byte = ((unsigned char *)&word)[0];
	pthread_join(thread, NULL);
	return byte;
}
