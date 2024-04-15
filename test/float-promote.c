void g1(int, ...);
void g2(float);
void f(void) {
	g1(0, 1.0f);
	g2(1.0f);
}
