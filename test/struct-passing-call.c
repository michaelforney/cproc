struct s {
	int x;
} s;

void f(struct s);
void g(void) {
	f(s);
}
