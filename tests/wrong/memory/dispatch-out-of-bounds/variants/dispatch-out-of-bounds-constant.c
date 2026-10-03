static int first(void) { return 1; }
static int second(void) { return 2; }

static int (*const table[2])(void) = {first, second};

int result;

int main(void)
{
	/* The constant index selects no slot, so the dispatch always fails and
	 * the rest of the block, including the second dispatch, is dead. */
	int value = table[5]();
	result = table[1]();
	return value;
}
