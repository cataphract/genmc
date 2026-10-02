#include <assert.h>
#include <stdint.h>

typedef int32_t int4 __attribute__((ext_vector_type(4)));
typedef uint8_t uchar4 __attribute__((ext_vector_type(4)));

static volatile int64_t signed_a = -7, signed_b = 3;
static volatile uint64_t unsigned_a = UINT64_MAX, unsigned_b = 3;
static volatile int8_t narrow_signed = -1;
static volatile uint8_t narrow_unsigned = 255;

int main(void)
{
	assert(__builtin_elementwise_min(signed_a, signed_b) == -7);
	assert(__builtin_elementwise_max(signed_a, signed_b) == 3);
	assert(__builtin_elementwise_min(unsigned_a, unsigned_b) == 3);
	assert(__builtin_elementwise_max(unsigned_a, unsigned_b) == UINT64_MAX);

	/* Narrow operands must keep their own signedness rather than be widened. */
	assert(__builtin_elementwise_min(narrow_signed, (int8_t)1) == -1);
	assert(__builtin_elementwise_max(narrow_unsigned, (uint8_t)1) == 255);

	int4 signed_vector = {signed_a, 1, -2, 4};
	int4 signed_min = __builtin_elementwise_min(signed_vector, (int4){0, 2, -3, 4});
	assert(signed_min.x == -7 && signed_min.y == 1 && signed_min.z == -3 && signed_min.w == 4);

	uchar4 unsigned_vector = {narrow_unsigned, 1, 7, 0};
	uchar4 unsigned_max = __builtin_elementwise_max(unsigned_vector, (uchar4){1, 2, 3, 0});
	assert(unsigned_max.x == 255 && unsigned_max.y == 2 && unsigned_max.z == 7 &&
	       unsigned_max.w == 0);
	return 0;
}
