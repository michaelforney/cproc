unsigned char u8 = u8'a';
static_assert(__builtin_types_compatible_p(typeof(u8'b'), unsigned char),
	"UTF-8 character constant has incorrect type");
