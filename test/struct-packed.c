struct [[gnu::packed]] {
	char a;
	long long b;
	short c;
} s = {1, 2, 3};

int main(void) {
	return s.a + s.b - s.c;
}
