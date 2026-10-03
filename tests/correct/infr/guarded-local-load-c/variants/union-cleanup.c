#include <stdbool.h>
#include <stddef.h>

int payload = 42;
int sink;
bool outcomes[2] = {false, true};

struct result {
	union {
		int *ptr;
		char err;
	} u;
	bool has;
};

/* noinline keeps clang from folding the result, and recursion keeps GenMC's
 * inliner from promoting it to registers. */
__attribute__((noinline)) static void make(struct result *r, int depth, bool ok)
{
	if (depth > 0) {
		make(r, depth - 1, ok);
		make(r, depth - 2, ok);
		return;
	}
	if (ok) {
		r->u.ptr = &payload;
		r->has = true;
	} else {
		r->u.err = 6;
		r->has = false;
	}
}

/* The source reads the pointer only if the discriminator says it is stored,
 * but the optimizer loads it unconditionally and masks the comparison. */
__attribute__((noinline)) static void consume(bool ok)
{
	struct result r;
	make(&r, 1, ok);
	if (r.has && r.u.ptr != NULL)
		sink = *r.u.ptr;
}

int main(void)
{
	consume(outcomes[0]);
	consume(outcomes[1]);
	return 0;
}
