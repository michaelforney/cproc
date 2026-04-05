int x;
unsigned y;

/* the bitint has the same width as x */
typeof(x + (_BitInt(sizeof x * 8))0) x;
typeof(x + (unsigned _BitInt(sizeof x * 8))0) y;
typeof(x + (unsigned _BitInt(sizeof x * 8 - 1))0) x;

_BitInt(5) z;
_BitInt(11) w;
typeof(z + w) w;
