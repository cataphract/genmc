#include <stdint.h>

uint64_t output;

int main(void)
{
	output = UINT64_C(0x1111111111111111);
	unsigned char *bytes = (unsigned char *)&output;
	/* The earlier write starts before the read's address. */
	return bytes[3] != 0x11;
}
