#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "cc.h"

struct token tok;

static char *tokstr[] = {
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
tokprint(const struct token *t)
{
	const char *str;

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
