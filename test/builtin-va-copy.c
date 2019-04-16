void f(void) {
	static __builtin_va_list a, b;
	__builtin_va_copy(a, b);
}
