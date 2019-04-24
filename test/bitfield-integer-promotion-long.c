struct {unsigned long x:31;} s1;
struct {unsigned long x:32;} s2;
struct {unsigned long x:33;} s3;
int c1 = __builtin_types_compatible_p(__typeof__(+s1.x), int);
int c2 = __builtin_types_compatible_p(__typeof__(+s2.x), unsigned);
int c3 = __builtin_types_compatible_p(__typeof__(+s3.x), unsigned long);
