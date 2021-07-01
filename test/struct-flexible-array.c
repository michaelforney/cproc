struct s {
	int a;
	short b[];
};

int x = sizeof(struct s);

int f(struct s *s) {
	return s->b[2];
}
