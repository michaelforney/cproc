int f(int i, ...) {
	int r, c = 0;
	__builtin_va_list ap;

	__builtin_va_start(ap, i);
	r = **__builtin_va_arg(ap, int (*)[++i]);
	__builtin_va_end(ap);
	return r + i;
}

int main(void) {
	int a[3];

	a[0] = 123;
	return f(3, &a) != 127;
}
