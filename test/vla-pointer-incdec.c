int g(void) {
	return 4;
}
void f(void) {
	typedef int a[g()];
	a x[2];
	a *y = x;
	++y;
	--y;
}
