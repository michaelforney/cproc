int l;
int f(int x) {
	l += x;
	return x;
}
int main(void) {
	int r = 0;
	int (*p[f(2)])[f(3)];
	r += l != 5;
	r += sizeof p != sizeof(int (*[2])[3]);
	r += sizeof **p != sizeof(int[3]);
	return r;
}
