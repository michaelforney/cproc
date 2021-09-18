struct {
	unsigned short s[6];
} u = {
	.s = u"aα€😐",
	.s[2] = u'£',
};

struct {
	unsigned s[5];
} U = {
	.s = U"aα€😐",
	.s[3] = U'😃',
};

struct {
	__typeof__(L' ') s[5];
} L = {
	.s = L"aα€😐",
	.s[3] = L'😃',
};
