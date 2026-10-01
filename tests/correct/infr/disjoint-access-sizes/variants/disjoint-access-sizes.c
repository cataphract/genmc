#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>

struct {
	uint64_t word;
	unsigned char byte;
	_Atomic unsigned short atomic_word;
} data;

int main(void)
{
	data.word = 42;
	data.byte = 7;
	atomic_store_explicit(&data.atomic_word, 11, memory_order_relaxed);
	assert(data.word == 42);
	assert(data.byte == 7);
	assert(atomic_load_explicit(&data.atomic_word, memory_order_relaxed) == 11);
	data.word = 43;
	data.byte = 8;
	assert(data.word == 43);
	assert(data.byte == 8);
	return 0;
}
