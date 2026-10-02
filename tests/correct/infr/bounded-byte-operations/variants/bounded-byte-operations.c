#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

static volatile unsigned length;

int main(void)
{
	static const unsigned char source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	for (unsigned n = 0; n <= 8; ++n) {
		length = n;
		uint64_t copied = 0;
		memcpy(&copied, source, length);
		unsigned char expected[8] = {0};
		for (unsigned i = 0; i < n; ++i)
			expected[i] = source[i];
		assert(memcmp(&copied, expected, sizeof(copied)) == 0);
		assert(memcmp(&copied, source, length) == 0);
	}
	unsigned char low[3] = {0x80, 0x01, 0xFF};
	unsigned char high[3] = {0x80, 0x02, 0x00};
	length = 0;
	assert(memcmp(low, high, length) == 0);
	length = 3;
	assert(memcmp(low, high, length) < 0);
	assert(memcmp(high, low, length) > 0);
	assert(bcmp(low, low, length) == 0);
	assert(bcmp(low, high, length) != 0);
	return 0;
}
