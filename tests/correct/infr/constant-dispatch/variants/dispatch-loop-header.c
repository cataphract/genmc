#include <assert.h>

static int one(void) { return 1; }
static int zero(void) { return 0; }

static int (*const table[3])(void) = {one, one, zero};

/* The promoted dispatch ends the loop header, whose condition loop jump
 * threading evaluates for the loop's initial index. */
int main(void)
{
	int i;
	for (i = 0; table[i](); i++)
		;
	assert(i == 2);
	return 0;
}
