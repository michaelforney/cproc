#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "cc.h"

struct token tok;

const char *tokstr[] = {
	/* keyword */
	[TAUTO] = "auto",
	[TBREAK] = "break",
	[TCASE] = "case",
	[TCHAR] = "char",
	[TCONST] = "const",
	[TCONTINUE] = "continue",
	[TDEFAULT] = "default",
	[TDO] = "do",
	[TDOUBLE] = "double",
	[TELSE] = "else",
	[TENUM] = "enum",
	[TEXTERN] = "extern",
	[TFLOAT] = "float",
	[TFOR] = "for",
	[TGOTO] = "goto",
	[TIF] = "if",
	[TINLINE] = "inline",
	[TINT] = "int",
	[TLONG] = "long",
	[TREGISTER] = "register",
	[TRESTRICT] = "restrict",
	[TRETURN] = "return",
	[TSHORT] = "short",
	[TSIGNED] = "signed",
	[TSIZEOF] = "sizeof",
	[TSTATIC] = "static",
	[TSTRUCT] = "struct",
	[TSWITCH] = "switch",
	[TTYPEDEF] = "typedef",
	[TUNION] = "union",
	[TUNSIGNED] = "unsigned",
	[TVOID] = "void",
	[TVOLATILE] = "volatile",
	[TWHILE] = "while",
	[T_ALIGNAS] = "_Alignas",
	[T_ALIGNOF] = "_Alignof",
	[T_ATOMIC] = "_Atomic",
	[T_BOOL] = "_Bool",
	[T_COMPLEX] = "_Complex",
	[T_GENERIC] = "_Generic",
	[T_IMAGINARY] = "_Imaginary",
	[T_NORETURN] = "_Noreturn",
	[T_STATIC_ASSERT] = "_Static_assert",
	[T_THREAD_LOCAL] = "_Thread_local",
	[T__ASM__] = "__asm__",
	[T__ATTRIBUTE__] = "__attribute__",
	[T__TYPEOF__] = "__typeof__",

	/* punctuator */
	[TLBRACK] = "[",
	[TRBRACK] = "]",
	[TLPAREN] = "(",
	[TRPAREN] = ")",
	[TLBRACE] = "{",
	[TRBRACE] = "}",
	[TPERIOD] = ".",
	[TARROW] = "->",
	[TINC] = "++",
	[TDEC] = "--",
	[TBAND] = "&",
	[TMUL] = "*",
	[TADD] = "+",
	[TSUB] = "-",
	[TBNOT] = "~",
	[TLNOT] = "!",
	[TDIV] = "/",
	[TMOD] = "%",
	[TSHL] = "<<",
	[TSHR] = ">>",
	[TLESS] = "<",
	[TGREATER] = ">",
	[TLEQ] = "<=",
	[TGEQ] = ">=",
	[TEQL] = "==",
	[TNEQ] = "!=",
	[TXOR] = "^",
	[TBOR] = "|",
	[TLAND] = "&&",
	[TLOR] = "||",
	[TQUESTION] = "?",
	[TCOLON] = ":",
	[TCOLONCOLON] = "::",
	[TSEMICOLON] = ";",
	[TELLIPSIS] = "...",
	[TASSIGN] = "=",
	[TMULASSIGN] = "*=",
	[TDIVASSIGN] = "/=",
	[TMODASSIGN] = "%=",
	[TADDASSIGN] = "+=",
	[TSUBASSIGN] = "-=",
	[TSHLASSIGN] = "<<=",
	[TSHRASSIGN] = ">>=",
	[TBANDASSIGN] = "&=",
	[TXORASSIGN] = "^=",
	[TBORASSIGN] = "|=",
	[TCOMMA] = ",",
	[THASH] = "#",
	[THASHHASH] = "##",
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
	bool quote;

	switch (kind) {
	case TIDENT:     class = "identifier"; quote = true;  break;
	case TNUMBER:    class = "number";     quote = true;  break;
	case TCHARCONST: class = "character";  quote = false; break;
	case TSTRINGLIT: class = "string";     quote = false; break;
	case TNEWLINE:   class = "newline";    quote = true;  break;
	default:
		class = NULL;
		lit = kind < LEN(tokstr) ? tokstr[kind] : NULL;
	}
	if (class && lit)
		snprintf(buf, len, quote ? "%s '%s'" : "%s %s", class, lit);
	else if (class)
		snprintf(buf, len, "%s", class);
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

_Noreturn void error(const struct location *loc, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%zu:%zu: error: ", loc->file, loc->line, loc->col);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
	exit(1);
}
