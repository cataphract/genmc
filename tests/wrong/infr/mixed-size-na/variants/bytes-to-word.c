#include <stdint.h>

/* Keep the storage global so SROA cannot split the final wide load. */
uint64_t output;

int main(void)
{
	unsigned char *bytes = (unsigned char *)&output;
	bytes[0] = 86;
	bytes[1] = 65;
	bytes[2] = 76;
	bytes[3] = 85;
	bytes[4] = 69;
	bytes[5] = 45;
	bytes[6] = 86;
	bytes[7] = 49;
	/* Reject the mixed-size load instead of incorrectly returning 86. */
	return output != UINT64_C(3555078731662639446);
}
