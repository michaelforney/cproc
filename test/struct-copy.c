struct s {
	int i;
	char s[5];
	float f;
} x = {123};

int main(void) {
	struct s y = x;
	return y.i != 123;
}
