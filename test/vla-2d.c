int g(void) {
	return 4;
}
int h(void) {
	return 3;
}
void f(void) {
	int x[g()][h()];
	int *y = &x[3][2];
	int z = y - x[0];
}
