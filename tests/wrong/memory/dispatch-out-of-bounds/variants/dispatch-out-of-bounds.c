static volatile unsigned long index = 2;

static int first(void) { return 1; }
static int second(void) { return 2; }

static int (*const table[2])(void) = {first, second};

int main(void)
{
	/* Promoting the table's targets must not turn this out-of-bounds load
	 * into a call to one of them. */
	return table[index]();
}
