void f(__builtin_va_list ap);
void g(void) {
	static __builtin_va_list ap;
	f(ap);
}
