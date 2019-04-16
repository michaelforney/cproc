struct {int x, y;} g(void);
int f(void) {
	return g().y;
}
