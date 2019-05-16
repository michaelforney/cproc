int main(void) {
	int x = 2, y = 0;
	do {
		if (x == 1)
			continue;
		++y;
	} while (x--);
	return y != 2;
}
