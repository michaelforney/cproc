unsigned char u8 = u8'a';
_Static_assert(__builtin_types_compatible_p(__typeof__(u8'b'), unsigned char),
	"UTF-8 character constant has incorrect type");
