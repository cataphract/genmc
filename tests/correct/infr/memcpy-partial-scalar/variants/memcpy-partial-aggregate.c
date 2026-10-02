#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct padded {
	unsigned char head;
	uint64_t tail;
};

static volatile uint64_t source_value = UINT64_C(0x1111111111111111);
static volatile uint64_t destination_value = UINT64_C(0x2222222222222222);

int main(void)
{
	struct padded source = {0x11, source_value};
	struct padded destination = {0x22, destination_value};

	/* The requested prefix includes padding, then one byte of tail. */
	memcpy(&destination, &source, offsetof(struct padded, tail) + 1);
	assert(destination.head == 0x11);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	assert(destination.tail == UINT64_C(0x2222222222222211));
#else
	assert(destination.tail == UINT64_C(0x1122222222222222));
#endif

	/* A prefix ending in padding must not reach the next field. */
	unsigned char raw_source[sizeof(struct padded)];
	memset(raw_source, 0x11, sizeof(raw_source));
	struct padded padding_destination = {0x22, destination_value};
	memcpy(&padding_destination, raw_source, offsetof(struct padded, tail) - 1);
	assert(padding_destination.head == 0x11);
	assert(padding_destination.tail == UINT64_C(0x2222222222222222));
	return 0;
}
