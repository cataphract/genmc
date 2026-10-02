#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint64_t source = UINT64_C(0x0102030405060708);
static uint64_t destination = UINT64_C(0x2222222222222222);

union word {
	uint64_t whole;
	uint32_t halves[2];
};
static union word word_source = {.whole = UINT64_C(0x0102030405060708)};
static union word word_destination = {.whole = UINT64_C(0x2222222222222222)};

int main(void)
{
	/* This remains a memory copy: SROA cannot hide a widened scalar access. */
	memcpy(&destination, &source, 3);
	unsigned char *bytes = (unsigned char *)&destination;
	unsigned char *source_bytes = (unsigned char *)&source;
	for (unsigned i = 0; i < sizeof(destination); ++i)
		assert(bytes[i] == (i < 3 ? source_bytes[i] : 0x22));

	/* An ordinary-width prefix must still support loads of that width. */
	memcpy(&word_destination.whole, &word_source.whole, sizeof(uint32_t));
	assert(word_destination.halves[0] == word_source.halves[0]);
	assert(word_destination.halves[1] == UINT32_C(0x22222222));
	return 0;
}
