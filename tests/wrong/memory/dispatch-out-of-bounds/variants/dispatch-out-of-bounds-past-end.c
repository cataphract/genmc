volatile unsigned long position = 2;
volatile int call = 0;

static int first(void) { return 1; }
static int second(void) { return 2; }

static int (*const table[2])(void) = {first, second};

int main(void)
{
	/* The table is the last global, so this read is past the end of static
	 * memory. Promoting the dispatch must not allocate anything there. */
	int (*f)(void) = table[position];
	if (call)
		return f();
	return 0;
}
