struct s {
	char s[5];
	float f;
} x;

void f(void) {
	struct s y = x;
}
