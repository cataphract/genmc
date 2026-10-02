#include <string.h>

static volatile unsigned length = 2;

int main(void)
{
	static const unsigned char source[2] = {1, 2};
	unsigned char destination[2];
	/* The bounds check must work even without any assertion in the input. */
	memcpy(destination, source, length);
	return memcmp(destination, source, length);
}
