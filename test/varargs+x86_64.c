void f(int n, ...) {
	__builtin_va_list ap;

	__builtin_va_start(ap, n);
	while (n) {
		__builtin_va_arg(ap, int);
		__builtin_va_arg(ap, float);
		__builtin_va_arg(ap, char *);
		--n;
	}
	__builtin_va_end(ap);
}
