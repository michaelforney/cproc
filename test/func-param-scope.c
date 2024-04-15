enum {A = 1};
char (*f(enum {A = 2} *p, int (*a)[A], double (*b)[sizeof **a]))[A] {
	static_assert(A == 2);
	static_assert(sizeof *b == sizeof(int) * sizeof(double));
	static_assert(sizeof *a == 2 * sizeof(int));
	return 0;
}
static_assert(A == 1);
static_assert(sizeof *f(0, 0, 0) == 1);
