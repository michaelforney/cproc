void f1(void) {
	static __builtin_va_list a, b;
	__builtin_va_copy(a, b);
}
void f2(__builtin_va_list b) {
	static __builtin_va_list a;
	__builtin_va_copy(a, b);
}
void f3(__builtin_va_list a, __builtin_va_list b) {
	__builtin_va_copy(a, b);
}
