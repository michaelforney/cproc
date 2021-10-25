/* applies to a and b */
[[dummy]] int a, b;

/* applies only to d */
int c, d [[dummy]];

/* applies to type int in this instance */
int [[dummy]] x;

/* applies to array type */
int array[8] [[dummy]];

/* applies to function type */
int function(void) [[dummy]];

/* applies to pointer type */
int *[[dummy]]const pointer;

/* standalone attribute is implementation-defined */
[[dummy]];

/* applies to type `struct s`, here and elsewhere */
struct [[dummy]] s {
	/* applies to i */
	[[dummy]] int i;

	/* applies to type float in this instance */
	float [[dummy]] f;
};

/* applies to type `union u`, here and elsewhere */
union [[dummy]] u {
	/* applies to i */
	[[dummy]] int i;

	/* applies to type float in this instance */
	float [[dummy]] f;
};

/* applies to type `enum e`, here and elsewhere */
enum [[dummy]] e {
	/* applies to E1 */
	E1 [[dummy]],

	/* applies to E2 */
	E2 [[dummy]] = 123,
};

/* applies to function */
[[dummy]] void f([[dummy]] int p1 /* applies to p1 */, int [[dummy]] p2 /* applies to type int */) {
	/* applies to type int */
	sizeof(int [[dummy]]);

	/* applies to array type */
	sizeof(int[4] [[dummy]]);

	/* applies to function type */
	sizeof(int (*)(float) [[dummy]]);

	/* applies to pointer type */
	sizeof(int *[[dummy]]);

	/* applies to statement */
	[[dummy]]
	if (0)
		;

	/* applies to label */
	[[dummy]]
	L1: 0;

	/* applies to statement */
	L2: [[dummy]]0;
}

[[dummy(attribute with parameters [123 456 ({+ -} if else)])]];
[[dummy1,dummy2,,dummy3]];
[[]];
[[dummy1]] [[dummy2]];
