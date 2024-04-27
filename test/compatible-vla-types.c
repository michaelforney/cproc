void f1(int n, int (*a)[n], int (*b)[*], int (*c)[3],
	struct {
		int x;
		static_assert(__builtin_types_compatible_p(typeof(a), typeof(b)));
		static_assert(__builtin_types_compatible_p(typeof(a), int (*)[3]));
		static_assert(__builtin_types_compatible_p(typeof(b), int (*)[3]));
		static_assert(__builtin_types_compatible_p(typeof(a), int (*)[]));
		static_assert(__builtin_types_compatible_p(typeof(b), int (*)[]));
	} s);
void f2(void) {
	int n = 12, m = 6 * 2;
	static_assert(__builtin_types_compatible_p(int [n], int [12]));
	static_assert(__builtin_types_compatible_p(int [], int [n]));
	static_assert(__builtin_types_compatible_p(int [n], int [m]));
	static_assert(__builtin_types_compatible_p(int [2][n], int [1 + 1][n]));
	static_assert(!__builtin_types_compatible_p(int [4][n], int [5][n]));
}
