#include <assert.h>
#include <stdatomic.h>

atomic_int selector;
atomic_int sink;

#define EACH(F)                                                                                    \
	F(0) F(1) F(2) F(3) F(4) F(5) F(6) F(7) F(8) F(9) F(10) F(11) F(12) F(13) F(14) F(15)
#define LIST(f)                                                                                    \
	f##0, f##1, f##2, f##3, f##4, f##5, f##6, f##7, f##8, f##9, f##10, f##11, f##12, f##13,    \
		f##14, f##15

#define LEAF(i)                                                                                    \
	static int leaf##i(int v)                                                                  \
	{                                                                                          \
		atomic_store(&sink, v);                                                            \
		return v + i;                                                                      \
	}
EACH(LEAF)
static int (*const leaves[16])(int) = {LIST(leaf)};

#define DISPATCH(table, i) table[(atomic_load(&selector) + i) & 15](v + i)
#define THIRD(i)                                                                                   \
	static int third##i(int v) { return DISPATCH(leaves, i); }
EACH(THIRD)
static int (*const thirds[16])(int) = {LIST(third)};
#define SECOND(i)                                                                                  \
	static int second##i(int v) { return DISPATCH(thirds, i); }
EACH(SECOND)
static int (*const seconds[16])(int) = {LIST(second)};
#define FIRST(i)                                                                                   \
	static int first##i(int v) { return DISPATCH(seconds, i); }
EACH(FIRST)
static int (*const firsts[16])(int) = {LIST(first)};

/* Every handler dispatches to the handlers of the next level. Promoting and
 * inlining all of these dispatches would copy each of the 16^4 paths into
 * main, so the promotions have to stop once they add too much code. */
int main(void)
{
	assert(firsts[atomic_load(&selector) + 1](1) == 5);
	return 0;
}
