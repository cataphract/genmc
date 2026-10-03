static volatile unsigned long index = 1000;

static int first(void) { return 1; }
static int second(void) { return 2; }

static int (*const table[2])(void) = {first, second};

int main(void)
{
	/* The table read itself is invalid, so it must be reported as such
	 * before the promoted dispatch rejects its index. */
	return table[index]();
}
