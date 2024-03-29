int main(void) {
	alignas(16) char x;
	return (unsigned long)&x % 16;
}
