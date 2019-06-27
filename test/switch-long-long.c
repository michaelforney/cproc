int main(void) {
	switch (0x12300000000) {
	case 0: return 1;
	case 0x12300000000: return 0;
	}
	return 2;
}
