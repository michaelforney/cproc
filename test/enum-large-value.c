enum E1 {
	A1 = 0x80000000,
	B1 = 0x80000000ll,
	A1type = __builtin_types_compatible_p(typeof(A1), unsigned),
	B1type = __builtin_types_compatible_p(typeof(B1), long long),
};
_Static_assert(__builtin_types_compatible_p(typeof(A1), unsigned));
_Static_assert(A1type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B1), unsigned));
_Static_assert(B1type == 1);
_Static_assert(__builtin_types_compatible_p(enum E1, unsigned));
_Static_assert(!__builtin_types_compatible_p(enum E1, enum { A1_ = A1, B1_ = B1 }));

enum E2 {
	A2 = 0x80000000,
	B2 = -1ll,
	A2type = __builtin_types_compatible_p(typeof(A2), unsigned),
	B2type = __builtin_types_compatible_p(typeof(B2), int),
};
_Static_assert(__builtin_types_compatible_p(typeof(A2), long));
_Static_assert(A2type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B2), long));
_Static_assert(B2type == 1);
_Static_assert(__builtin_types_compatible_p(enum E2, long));
_Static_assert(!__builtin_types_compatible_p(enum E2, enum { A2_ = A2, B2_ = B2 }));

enum E3 {
	A3 = 0x7fffffff,
	B3,
	A3type = __builtin_types_compatible_p(typeof(A3), int),
	B3type = __builtin_types_compatible_p(typeof(B3), long),
};
_Static_assert(__builtin_types_compatible_p(typeof(A3), unsigned));
_Static_assert(A3type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B3), unsigned));
_Static_assert(B3type == 1);
_Static_assert(__builtin_types_compatible_p(enum E3, unsigned));
_Static_assert(!__builtin_types_compatible_p(enum E3, enum { A3_ = A3, B3_ }));

enum E4 {
	A4 = -0x80000001l,
	B4,
	C4 = B4,
	A4type = __builtin_types_compatible_p(typeof(A4), long),
	B4type = __builtin_types_compatible_p(typeof(B4), long),
	C4type = __builtin_types_compatible_p(typeof(C4), int),
};
_Static_assert(__builtin_types_compatible_p(typeof(A4), long));
_Static_assert(A4type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B4), long));
_Static_assert(B4type == 1);
_Static_assert(__builtin_types_compatible_p(enum E4, long));
_Static_assert(!__builtin_types_compatible_p(enum E4, enum { A4_ = A4, B4_, C4 = C4 }));

enum E5 {
	A5 = 0x100000000,
	B5 = -1ull,
	A5type = __builtin_types_compatible_p(typeof(A5), long),
	B5type = __builtin_types_compatible_p(typeof(B5), unsigned long long),
};
_Static_assert(__builtin_types_compatible_p(typeof(A5), unsigned long));
_Static_assert(A5type == 1);
_Static_assert(__builtin_types_compatible_p(typeof(B5), unsigned long));
_Static_assert(B5type == 1);
_Static_assert(__builtin_types_compatible_p(enum E5, unsigned long));
_Static_assert(!__builtin_types_compatible_p(enum E5, enum { A5_ = A5, B5_ = B5 }));
