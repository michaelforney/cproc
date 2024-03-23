enum E1 {
	A1 = 1u,
	B1 = -1,
	A1type = __builtin_types_compatible_p(typeof(A1), int),
	B1type = __builtin_types_compatible_p(typeof(B1), int),
};
_Static_assert(__builtin_types_compatible_p(typeof(A1), int));
_Static_assert(A1type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B1), int));
_Static_assert(B1type == 1);
_Static_assert(__builtin_types_compatible_p(enum E1, int));

enum E2 {
	A2 = 1,
	B2 = 2,
	A2type = __builtin_types_compatible_p(typeof(A2), int),
	B2type = __builtin_types_compatible_p(typeof(B2), int),
};
_Static_assert(__builtin_types_compatible_p(typeof(A2), int));
_Static_assert(A2type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B2), int));
_Static_assert(B2type == 1);
_Static_assert(__builtin_types_compatible_p(enum E2, unsigned));
