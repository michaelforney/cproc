void f(void) {
	struct {
		char s[6];
	} x = {
		.s[0] = 'x',
		.s[4] = 'y',
		.s = "hello",
		.s[1] = 'a',
	};
}
