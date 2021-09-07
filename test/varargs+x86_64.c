int f1(int n, __builtin_va_list ap) {
	return __builtin_va_arg(ap, int);
}

int f2(int n, ...) {
	int r;
	__builtin_va_list ap;

	__builtin_va_start(ap, n);
	r = f1(n, ap);
	__builtin_va_end(ap);
	return r;
}

void f3(int n, ...) {
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
