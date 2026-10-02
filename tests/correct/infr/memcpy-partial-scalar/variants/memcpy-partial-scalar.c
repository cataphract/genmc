#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
	static uint64_t source = UINT64_C(0x1111111111111111);
	static uint64_t destination = UINT64_C(0x2222222222222222);
	memcpy(&destination, &source, 1);
	unsigned char *bytes = (unsigned char *)&destination;
	assert(bytes[0] == 0x11);
	for (unsigned i = 1; i < sizeof(destination); ++i)
		assert(bytes[i] == 0x22);

	/* SROA must preserve the suffix when an odd-sized scalar prefix is copied. */
	uint64_t local_source = UINT64_C(0x1111111111111111);
	uint64_t local_destination = UINT64_C(0x2222222222222222);
	memcpy(&local_destination, &local_source, 3);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	assert(local_destination == UINT64_C(0x2222222222111111));
#else
	assert(local_destination == UINT64_C(0x1111112222222222));
#endif
	memcpy(&local_destination, &local_source, 0);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	assert(local_destination == UINT64_C(0x2222222222111111));
#else
	assert(local_destination == UINT64_C(0x1111112222222222));
#endif
	return 0;
}
