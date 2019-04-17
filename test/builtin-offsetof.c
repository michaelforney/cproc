struct s {
	struct {
		union {
			float z;
			char *c;
		} b;
	} a[5];
};
int x = __builtin_offsetof(struct s, a[2].b.c);
