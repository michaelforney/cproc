int main(void) {
	_Alignas(16) char x;
	return (unsigned long)&x % 16;
}
