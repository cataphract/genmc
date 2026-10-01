#include <stdint.h>

union {
	uint64_t word;
	uint32_t halves[2];
} data;

int main(void)
{
	data.word = UINT64_C(0x1111111111111111);
	data.halves[0] = UINT32_C(0x22222222);
	/* This surviving four-byte segment still came from an eight-byte write. */
	return data.halves[1] != UINT32_C(0x11111111);
}
