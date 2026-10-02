#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct interior {
	unsigned char head;
	uint64_t tail;
};

struct trailing {
	uint64_t head;
	unsigned char tail;
};

int main(void)
{
	/* Fields written individually leave heap padding uninitialized. Copying
	 * the whole object must not read those bytes. */
	struct interior *interior = malloc(sizeof(*interior));
	interior->head = 0x11;
	interior->tail = UINT64_C(0x2222222222222222);
	struct interior interiorCopy;
	memcpy(&interiorCopy, interior, sizeof(interiorCopy));
	assert(interiorCopy.head == 0x11 && interiorCopy.tail == UINT64_C(0x2222222222222222));

	struct trailing *trailing = malloc(sizeof(*trailing));
	trailing->head = UINT64_C(0x3333333333333333);
	trailing->tail = 0x44;
	struct trailing trailingCopy;
	memcpy(&trailingCopy, trailing, sizeof(trailingCopy));
	assert(trailingCopy.head == UINT64_C(0x3333333333333333) && trailingCopy.tail == 0x44);

	free(interior);
	free(trailing);
	return 0;
}
