enum E1 : short;
_Static_assert(__builtin_types_compatible_p(enum E1, short));

enum E2 : unsigned short {
	A2 = 0x7fff,
	B2,
	A2type = __builtin_types_compatible_p(typeof(A2), unsigned short),
	B2type = __builtin_types_compatible_p(typeof(B2), unsigned short),
};
_Static_assert(__builtin_types_compatible_p(typeof(A2), unsigned short));
_Static_assert(A2type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B2), unsigned short));
_Static_assert(B2type == 1);
_Static_assert(__builtin_types_compatible_p(enum E2, unsigned short));

enum E3 : long long {
	A3,
	B3,
	A3type = __builtin_types_compatible_p(typeof(A3), long long),
	B3type = __builtin_types_compatible_p(typeof(B3), long long),
};
_Static_assert(__builtin_types_compatible_p(typeof(A3), long long));
_Static_assert(A3type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B3), long long));
_Static_assert(B3type == 1);
_Static_assert(__builtin_types_compatible_p(enum E3, long long));

enum E4 : long long {
	A4 = sizeof(enum E4),
	A4type1 = __builtin_types_compatible_p(typeof(A4), enum E4),
	A4type2 = !__builtin_types_compatible_p(typeof(A4), enum E3),
};
_Static_assert(__builtin_types_compatible_p(typeof(A4), enum E4));
_Static_assert(A4type1 == 1);
_Static_assert(!__builtin_types_compatible_p(typeof(A4), enum E3));
_Static_assert(A4type2 == 1);
