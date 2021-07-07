#include <uchar.h>
#include "utf.h"

size_t
utf8enc(char32_t c, char *s)
{
	if (c < 0x80) {
		s[0] = c;
		return 1;
	}
	if (c < 0x800) {
		s[0] = 0xc0 | c >> 6;
		s[1] = 0x80 | c & 0x3f;
		return 2;
	}
	if (c < 0xd800 || c - 0xe000 < 0x2000) {
		s[0] = 0xe0 | c >> 12;
		s[1] = 0x80 | c >> 6 & 0x3f;
		s[2] = 0x80 | c & 0x3f;
		return 3;
	}
	if (c - 0x10000 < 0x100000) {
		s[0] = 0xf0 | c >> 18;
		s[1] = 0x80 | c >> 12 & 0x3f;
		s[2] = 0x80 | c >> 6 & 0x3f;
		s[3] = 0x80 | c & 0x3f;
		return 4;
	}
	return -1;
}

size_t
utf8dec(const char *s, size_t n, char32_t *c)
{
	size_t i, l;
	unsigned char b;
	char32_t x;

	b = s[0];
	if (b < 0x80) {
		*c = b;
		return 1;
	}
	if ((b & 0xe0) == 0xc0) {
		x = b & 0x1f;
		l = 2;
	} else if ((b & 0xf0) == 0xe0) {
		x = b & 0x0f;
		l = 3;
	} else if ((b & 0xf8) == 0xf0) {
		x = b & 0x07;
		l = 4;
	}
	if (n < l)
		return -1;
	for (i = 1; i < l; ++i) {
		b = *++s;
		if ((b & 0xc0) != 0x80)
			return -1;
		x = x << 6 | b & 0x3f;
	}
	*c = x;
	return l;
}

size_t
utf16enc(char32_t c, char16_t *s)
{
	if (c < 0xd800 || c - 0xe000 < 0x2000) {
		s[0] = c;
		return 1;
	}
	c -= 0x10000;
	if (c < 0x100000) {
		s[0] = 0xd800 | c >> 10 & 0x3ff;
		s[1] = 0xdc00 | c & 0x3ff;
		return 2;
	}
	return -1;
}
