#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "scan.h"
#include "token.h"

struct buffer {
	char *str;
	size_t len, cap;
};

struct scanner {
	int chr;
	struct token next;
	struct location loc;
	struct buffer buf;
	bool usebuf;
};

static void
bufadd(struct buffer *b, char c)
{
	if (b->len >= b->cap) {
		b->cap = b->cap ? b->cap * 2 : 1<<8;
		b->str = xreallocarray(b->str, b->cap, 1);
	}
	b->str[b->len++] = c;
}

static char *
bufget(struct buffer *b)
{
	char *s;

	bufadd(b, '\0');
	s = xmalloc(b->len);
	memcpy(s, b->str, b->len);
	b->len = 0;

	return s;
}

static void
nextchar(struct scanner *s)
{
	int c;

	if (s->usebuf)
		bufadd(&s->buf, s->chr);
	for (;;) {
		s->chr = getchar();
		if (s->chr == '\n')
			++s->loc.line, s->loc.col = 1;
		else
			++s->loc.col;
		if (s->chr != '\\')
			break;
		c = getchar();
		if (c != '\n') {
			ungetc(c, stdin);
			break;
		}
		++s->loc.line, s->loc.col = 1;
	}
}

static int
op2(struct scanner *s, int t1, int t2)
{
	nextchar(s);
	if (s->chr != '=')
		return t1;
	nextchar(s);
	return t2;
}

static int
op3(struct scanner *s, int t1, int t2, int t3)
{
	int c;

	c = s->chr;
	nextchar(s);
	if (s->chr == '=') {
		nextchar(s);
		return t2;
	}
	if (s->chr != c)
		return t1;
	nextchar(s);
	return t3;
}

static int
op4(struct scanner *s, int t1, int t2, int t3, int t4)
{
	int c;

	c = s->chr;
	nextchar(s);
	if (s->chr == '=') {
		nextchar(s);
		return t2;
	}
	if (s->chr != c)
		return t1;
	nextchar(s);
	if (s->chr != '=')
		return t3;
	nextchar(s);
	return t4;
}

static int
ident(struct scanner *s)
{
	s->usebuf = true;
	do nextchar(s);
	while (isalnum(s->chr) || s->chr == '_');

	return TIDENT;
}

static int
isodigit(int c)
{
	return (unsigned)c - '0' < 8;
}

static enum tokenkind
number(struct scanner *s)
{
	bool allowsign = false;

	s->usebuf = true;
	for (;;) {
		nextchar(s);
		switch (s->chr) {
		case 'e':
		case 'E':
		case 'p':
		case 'P':
			allowsign = true;
			break;
		case '+':
		case '-':
			if (!allowsign)
				goto done;
			break;
		case '_':
		case '.':
			allowsign = false;
			break;
		default:
			if (!isalnum(s->chr))
				goto done;
			allowsign = false;
		}
	}
done:
	return TNUMBER;
}

static void
escape(struct scanner *s)
{
	nextchar(s);
	if (s->chr == 'x') {
		nextchar(s);
		if (!isxdigit(s->chr))
			error(&s->loc, "invalid hexadecimal escape sequence");
		do nextchar(s);
		while (isxdigit(s->chr));
	} else if (isodigit(s->chr)) {
		nextchar(s);
		if (isodigit(s->chr)) {
			nextchar(s);
			if (isodigit(s->chr))
				nextchar(s);
		}
	} else if (strchr("'\"?\\abfnrtv", s->chr)) {
		nextchar(s);
	} else {
		error(&s->loc, "invalid escape sequence");
	}
}

static enum tokenkind
charconst(struct scanner *s)
{
	s->usebuf = true;
	nextchar(s);
	for (;;) {
		switch (s->chr) {
		case '\\':
			escape(s);
			break;
		case '\'':
			nextchar(s);
			return TCHARCONST;
		case '\n':
			error(&s->loc, "newline in character constant");
		default:
			nextchar(s);
			break;
		}
	}
}

static int
stringlit(struct scanner *s)
{
	s->usebuf = true;
	nextchar(s);
	for (;;) {
		switch (s->chr) {
		case '\\':
			escape(s);
			break;
		case '"':
			nextchar(s);
			return TSTRINGLIT;
		case '\n':
			error(&s->loc, "newline in string literal");
		default:
			nextchar(s);
			break;
		}
	}
}

static int
scankind(struct scanner *s)
{
	int tok;
	struct location loc;

again:
	switch (s->chr) {
	case ' ':
	case '\t':
	case '\f':
	case '\v':
		nextchar(s);
		goto again;
	case '!':
		return op2(s, TLNOT, TNEQ);
	case '"':
		return stringlit(s);
	case '#':
		nextchar(s);
		if (s->chr != '#')
			return THASH;
		nextchar(s);
		return THASHHASH;
	case '%':
		return op2(s, TMOD, TMODASSIGN);
	case '&':
		return op3(s, TBAND, TBANDASSIGN, TLAND);
	// TODO: u8, U, and L string literals
	case '\'':
		return charconst(s);
	case '*':
		return op2(s, TMUL, TMULASSIGN);
	case '+':
		return op3(s, TADD, TADDASSIGN, TINC);
	case '-':
		tok = op3(s, TSUB, TSUBASSIGN, TDEC);
		if (tok != TSUB || s->chr != '>')
			return tok;
		nextchar(s);
		return TARROW;
	case '/':
		return op2(s, TDIV, TDIVASSIGN);
	case '<':
		return op4(s, TLESS, TLEQ, TSHL, TSHLASSIGN);
	case '=':
		return op2(s, TASSIGN, TEQL);
	case '>':
		return op4(s, TGREATER, TGEQ, TSHR, TSHRASSIGN);
	case '^':
		return op2(s, TXOR, TXORASSIGN);
	case '|':
		return op3(s, TBOR, TBORASSIGN, TLOR);
	case '\n':
		nextchar(s);
		return TNEWLINE;
	case '[':
		nextchar(s);
		return TLBRACK;
	case ']':
		nextchar(s);
		return TRBRACK;
	case '(':
		nextchar(s);
		return TLPAREN;
	case ')':
		nextchar(s);
		return TRPAREN;
	case '{':
		nextchar(s);
		return TLBRACE;
	case '}':
		nextchar(s);
		return TRBRACE;
	case '.':
		nextchar(s);
		if (s->chr != '.')
			return TPERIOD;
		loc = s->loc;
		nextchar(s);
		if (s->chr != '.') {
			ungetc(s->chr, stdout);
			s->loc = loc;
			s->chr = '.';
			return TPERIOD;
		}
		nextchar(s);
		return TELLIPSIS;
	case '~':
		nextchar(s);
		return TBNOT;
	case '?':
		nextchar(s);
		return TQUESTION;
	case ':':
		nextchar(s);
		return TCOLON;
	case ';':
		nextchar(s);
		return TSEMICOLON;
	case ',':
		nextchar(s);
		return TCOMMA;
	case EOF:
		return TEOF;
	default:
		if (isdigit(s->chr))
			return number(s);
		if (isalpha(s->chr) || s->chr == '_')
			return ident(s);
		if (isprint(s->chr))
			error(&s->loc, "unexpected character '%c'", s->chr);
		error(&s->loc, "unexpected character '\\x%02x'", s->chr);
	}
}

struct scanner *
mkscanner(const char *file)
{
	struct scanner *s;

	s = xmalloc(sizeof(*s));
	s->buf.str = NULL;
	s->buf.len = 0;
	s->buf.cap = 0;
	s->usebuf = false;
	s->loc.file = file;
	s->loc.line = 1;
	s->loc.col = 0;
	nextchar(s);

	return s;
}

void
scan(struct scanner *s, struct token *t)
{
	t->loc = s->loc;
	t->kind = scankind(s);
	if (s->usebuf) {
		t->lit = bufget(&s->buf);
		s->usebuf = false;
	} else {
		t->lit = NULL;
	}
}
