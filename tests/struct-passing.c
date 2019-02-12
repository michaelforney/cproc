struct s {
	int x;
	struct {
		char y[3];
		short z;
	} s[2];
	double w;
};

void f(struct s s) {
}
