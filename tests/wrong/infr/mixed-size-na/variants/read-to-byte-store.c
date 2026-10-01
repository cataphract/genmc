#include <assert.h>
#include <stdint.h>

uint64_t output = UINT64_C(0x1111111111111111);

int main(void)
{
	assert(output == UINT64_C(0x1111111111111111));
	unsigned char *bytes = (unsigned char *)&output;
	/* Neither the earlier read nor the partial store is rejected. */
	bytes[3] = 0xaa;
	return output != UINT64_C(0x11111111aa111111);
}
