int c = 0;
int main(void) {
	int r = 0;
	int l = 2;
	int (*p)[l] = 0;
	r += sizeof(c++, p) != sizeof(int (*)[]);       /* VM, but not VLA */
	r += c != 0;
	r += sizeof(*(c++, p)) != 2 * sizeof(int);      /* VLA */
	r += c != 1;
	r += sizeof(c++, *p) != sizeof(int *);          /* VLA decayed to pointer */
	r += c != 1;
	r += sizeof(int[++l]) != 3 * sizeof(int);       /* VLA */
	r += l != 3;
	r += sizeof(int[++l][1]) != sizeof(int[4][1]);  /* VLA */
	r += l != 4;
	r += sizeof(int[(c++, 5)]) != 5 * sizeof(int);  /* VLA */
	r += c != 2;
	return r;
}
