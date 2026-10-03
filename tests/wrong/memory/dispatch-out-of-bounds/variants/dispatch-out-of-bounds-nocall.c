static volatile unsigned long index = 1000;
static volatile int call = 0;

static int first(void) { return 1; }
static int second(void) { return 2; }

static int (*const table[2])(void) = {first, second};

int main(void)
{
	/* Promoting the dispatch must keep the table read, which is out of
	 * bounds even though the call is never made. */
	int (*f)(void) = table[index];
	if (call)
		return f();
	return 0;
}
