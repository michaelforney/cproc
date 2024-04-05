int a[3] = {12, 34, 56};
int b[3] = {'a', 'b', 'c'};
int c = 0;
int f(void) {
	++c;
	return 3;
}
int main(void) {
	int r = 0;
	int (*p)[f()] = 0;
	r += c != 1;
	typeof(c++, p) t1;            /* VM; evaluated */
	r += c != 2;
	typeof(p, c++) t2;            /* non-VM; not evaluated */
	r += c != 2;
	typeof(c++, **(p = &a)) t3;   /* non-VM; not evaluated */
	r += c != 2;
	r += p != 0;
	typeof(*(p = (c++, &a))) t4;  /* VM, evaluated */
	r += c != 3;
	r += p != &a;
	/*
	while *(p = &b) has VM type, it is not the immediate operand
	of typeof, so is converted from VLA to int pointer due to
	the comma operator, so the typeof expression is not evaluated
	*/
	typeof(c++, *(p = &b)) w;
	r += c != 3;
	r += p != &a;
	return r;
}
