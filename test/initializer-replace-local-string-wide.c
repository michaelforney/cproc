void f(void) {
	struct {
		unsigned short u[6];
		unsigned U[6];
		__typeof__(L' ') L[6];
	} x = {
		.u[0] = u'x',
		.u[4] = u'y',
		.u = u"hello",
		.u[1] = u'a',

		.U[0] = U'x',
		.U[4] = U'y',
		.U = U"hello",
		.U[1] = U'a',

		.L[0] = L'x',
		.L[4] = L'y',
		.L = L"hello",
		.L[1] = L'a',
	};
}
