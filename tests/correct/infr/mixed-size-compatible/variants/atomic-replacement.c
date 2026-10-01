#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>

_Atomic uint64_t word;

int main(void)
{
	unsigned char *bytes = (unsigned char *)&word;
	for (unsigned i = 0; i < sizeof(word); ++i)
		bytes[i] = 0;
	atomic_store_explicit(&word, UINT64_C(0x1111111111111111), memory_order_relaxed);
	assert(atomic_load_explicit(&word, memory_order_relaxed) ==
	       UINT64_C(0x1111111111111111));
	assert(*(uint64_t *)&word == UINT64_C(0x1111111111111111));
	bytes[3] = 0xaa;
	assert(bytes[3] == 0xaa);
	atomic_store_explicit(&word, UINT64_C(0x2222222222222222), memory_order_relaxed);
	assert(*(uint64_t *)&word == UINT64_C(0x2222222222222222));
	return 0;
}
