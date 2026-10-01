#include <stdint.h>

uint64_t output;

int main(void)
{
	output = UINT64_C(0x1111111111111111);
	unsigned char *bytes = (unsigned char *)&output;
	/* The partial overwrite is valid; the following wide load is unsupported. */
	bytes[3] = 0xaa;
	return output != UINT64_C(0x11111111aa111111);
}
