int main(void) {
	int x = 1;
	return (++x ?: 0ull) != 2;
}
