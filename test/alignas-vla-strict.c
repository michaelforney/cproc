int n = 43;
int main(void) {
	char alignas(64) a[n];
	return (unsigned long)a % 64;
}
