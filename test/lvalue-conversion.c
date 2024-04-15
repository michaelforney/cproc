void g(int);
void f(void) {
	static const unsigned char c = 0;
	g(c);
	g(~c);
}
