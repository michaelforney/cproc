int main(void) {
	if ((0 ? 0 : 123) != 123)
		return 1;
	if ((1 ? 456 : 0) != 456)
		return 1;
}
