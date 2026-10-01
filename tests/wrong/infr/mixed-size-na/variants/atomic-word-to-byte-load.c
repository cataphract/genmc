#include <stdint.h>
#include <stdatomic.h>

_Atomic uint64_t word;

int main(void)
{
	atomic_store_explicit(&word, UINT64_C(0x1111111111111111), memory_order_relaxed);
	unsigned char *bytes = (unsigned char *)&word;
	return bytes[3] != 0x11;
}
