int main(void) {
	alignas(32) char x;
	return (unsigned long)&x % 32;
}
