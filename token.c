#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "cc.h"

struct token tok;

const char *tokstr[] = {
#define TOKEN(t, s) [t] = s,
#include "tokens.h"
#undef TOKEN
};

void
tokenprint(const struct token *t)
{
	const char *str;

	if (t->space)
		fputc(' ', stdout);
	switch (t->kind) {
	case TIDENT:
	case TNUMBER:
	case TCHARCONST:
	case TSTRINGLIT:
		str = t->lit;
		break;
	case TNEWLINE:
		str = "\n";
		break;
	case TEOF:
		return;
	default:
		str = tokstr[t->kind];
	}
	if (!str)
		fatal("cannot print token %d", t->kind);
	fputs(str, stdout);
}

static void
tokendesc(char *buf, size_t len, enum tokenkind kind, const char *lit)
{
	const char *class;
	bool quote = true;

	switch (kind) {
	case TEOF:       class = "EOF";                       break;
	case TIDENT:     class = "identifier"; quote = true;  break;
	case TNUMBER:    class = "number";     quote = true;  break;
	case TCHARCONST: class = "character";  quote = false; break;
	case TSTRINGLIT: class = "string";     quote = false; break;
	case TNEWLINE:   class = "newline";                   break;
	case TOTHER:     class = NULL;                        break;
	default:
		class = NULL;
		lit = kind < countof(tokstr) ? tokstr[kind] : NULL;
	}
	if (class && lit)
		snprintf(buf, len, quote ? "%s '%s'" : "%s %s", class, lit);
	else if (class)
		snprintf(buf, len, "%s", class);
	else if (kind == TOTHER && !isprint(*(unsigned char *)lit))
		snprintf(buf, len, "<U+%04x>", *(unsigned char *)lit);
	else if (lit)
		snprintf(buf, len, "'%s'", lit);
	else
		snprintf(buf, len, "<unknown>");
}

char *
tokencheck(const struct token *t, enum tokenkind kind, const char *msg)
{
	char want[64], got[64];

	if (t->kind != kind) {
		tokendesc(want, sizeof(want), kind, NULL);
		tokendesc(got, sizeof(got), t->kind, t->lit);
		error(&t->loc, "expected %s %s, saw %s", want, msg, got);
	}
	return t->lit;
}

void error(const struct location *loc, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%zu:%zu: error: ", loc->file, loc->line, loc->col);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
	exit(1);
}
