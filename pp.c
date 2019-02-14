#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "pp.h"
#include "scan.h"
#include "token.h"

static struct scanner *scanner;
static struct token pending;

static void
keyword(struct token *tok)
{
	static const struct {
		const char *name;
		int value;
	} keywords[] = {
		{"_Alignas",          T_ALIGNAS},
		{"_Alignof",          T_ALIGNOF},
		{"_Atomic",           T_ATOMIC},
		{"_Bool",             T_BOOL},
		{"_Complex",          T_COMPLEX},
		{"_Generic",          T_GENERIC},
		{"_Imaginary",        T_IMAGINARY},
		{"_Noreturn",         T_NORETURN},
		{"_Static_assert",    T_STATIC_ASSERT},
		{"_Thread_local",     T_THREAD_LOCAL},
		{"auto",              TAUTO},
		{"break",             TBREAK},
		{"case",              TCASE},
		{"char",              TCHAR},
		{"const",             TCONST},
		{"continue",          TCONTINUE},
		{"default",           TDEFAULT},
		{"do",                TDO},
		{"double",            TDOUBLE},
		{"else",              TELSE},
		{"enum",              TENUM},
		{"extern",            TEXTERN},
		{"float",             TFLOAT},
		{"for",               TFOR},
		{"goto",              TGOTO},
		{"if",                TIF},
		{"inline",            TINLINE},
		{"int",               TINT},
		{"long",              TLONG},
		{"register",          TREGISTER},
		{"restrict",          TRESTRICT},
		{"return",            TRETURN},
		{"short",             TSHORT},
		{"signed",            TSIGNED},
		{"sizeof",            TSIZEOF},
		{"static",            TSTATIC},
		{"struct",            TSTRUCT},
		{"switch",            TSWITCH},
		{"typedef",           TTYPEDEF},
		{"union",             TUNION},
		{"unsigned",          TUNSIGNED},
		{"void",              TVOID},
		{"volatile",          TVOLATILE},
		{"while",             TWHILE},
	};
	size_t low = 0, high = LEN(keywords), mid;
	int cmp;

	while (low < high) {
		mid = (low + high) / 2;
		cmp = strcmp(tok->lit, keywords[mid].name);
		if (cmp == 0) {
			free(tok->lit);
			tok->kind = keywords[mid].value;
			tok->lit = NULL;
			break;
		}
		if (cmp < 0)
			high = mid;
		else
			low = mid + 1;
	}
}

void
ppinit(const char *file)
{
	scanner = mkscanner(file);
	next();
}

static void
nextinto(struct token *t)
{
	do scan(scanner, t);
	while (t->kind == TNEWLINE);
	if (t->kind == TIDENT)
		keyword(t);
}

void
next(void)
{
	if (pending.kind) {
		tok = pending;
		pending.kind = TNONE;
	} else {
		nextinto(&tok);
	}
}

bool
peek(int kind)
{
	nextinto(&pending);
	if (pending.kind != kind)
		return false;
	pending.kind = TNONE;
	nextinto(&tok);
	return true;
}

char *
expect(int kind, const char *msg)
{
	char *lit;

	if (tok.kind != kind)
		error(&tok.loc, "expected %d %s, saw %d", kind, msg, tok.kind);
	lit = tok.lit;
	next();

	return lit;
}

bool
consume(int kind)
{
	if (tok.kind != kind)
		return false;
	next();
	return true;
}
