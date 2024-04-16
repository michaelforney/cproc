static_assert(__builtin_types_compatible_p(int[2], int[1 + 1]));
static_assert(!__builtin_types_compatible_p(int[2], int[1]));
static_assert(!__builtin_types_compatible_p(int[2], unsigned[2]));
static_assert(!__builtin_types_compatible_p(const int (*)[2], int (*)[2]));
typedef int T[2];
/* FIXME
static_assert(__builtin_types_compatible_p(const T *, const int (*)[2]));
*/
static_assert(__builtin_types_compatible_p(float[], float[3]));
