union [[gnu::packed]] u1 {
	int x;
	char y[5];
};
union u2 {
	int x;
	char y[5];
};
_Static_assert(sizeof(union u1) == 5);
_Static_assert(sizeof(union u2) == 8);
