int main(void) {
	_Alignas(32) char x;
	return (unsigned long)&x % 32;
}
