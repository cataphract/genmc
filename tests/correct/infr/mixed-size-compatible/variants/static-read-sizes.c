#include <assert.h>
#include <stdint.h>

uint64_t source = UINT64_C(0x1111111111111111);

int main(void)
{
	assert(source == UINT64_C(0x1111111111111111));
	unsigned char *bytes = (unsigned char *)&source;
	for (unsigned i = 0; i < sizeof(source); ++i)
		assert(bytes[i] == 0x11);
	assert(source == UINT64_C(0x1111111111111111));
	return 0;
}
