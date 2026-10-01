#include <assert.h>
#include <stdint.h>

uint64_t word = UINT64_C(0x1111111111111111);

int main(void)
{
	assert(word == UINT64_C(0x1111111111111111));
	word = UINT64_C(0x2222222222222222);
	unsigned char *bytes = (unsigned char *)&word;
	bytes[3] = 0xaa;
	assert(bytes[3] == 0xaa);
	/* A covering replacement makes the wide load safe again. */
	word = UINT64_C(0x3333333333333333);
	assert(word == UINT64_C(0x3333333333333333));
	/* A partial store with no incompatible later load is also accepted. */
	bytes[5] = 0xbb;
	return 0;
}
