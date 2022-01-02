#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "utf.h"

size_t
utf8enc(unsigned char *s, uint_least32_t c)
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
	assert(0);
	return 0;
}

size_t
utf8dec(uint_least32_t *c, const unsigned char *s, size_t n)
{
	size_t i, l;
	unsigned char b;
	uint_least32_t x;

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
	} else {
		return -1;
	}
	if (n < l)
		return -1;
	for (i = 1; i < l; ++i) {
		b = *++s;
		if ((b & 0xc0) != 0x80)
			return -1;
		x = x << 6 | b & 0x3f;
	}
	if (x >= 0x110000 || x - 0xd800 < 0x0200)
		return -1;
	*c = x;
	return l;
}

size_t
utf16enc(uint_least16_t *s, uint_least32_t c)
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
	assert(0);
	return 0;
}
