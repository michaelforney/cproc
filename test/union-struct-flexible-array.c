union u {
	struct {
		int a;
		short b[];
	} s;
	unsigned char storage[32];
};

int x = sizeof(union u);

int f(union u *u) {
	return u->s.b[2] + u->storage[0];
}
