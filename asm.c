#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "cc.h"

void
toplevelasm(void) {
	size_t asmsz;
	char *src;
	unsigned char *asmstr, *dst;

	asmsz = 0;
	asmstr = NULL;

	expect(T__ASM__, "at top level");
	expect(TLPAREN, "after __asm__");
	if (tok.kind != TSTRINGLIT)
		error(&tok.loc, "expected a string literal");
	do {
		asmstr = xreallocarray(asmstr, asmsz + strlen(tok.lit) + 1, 1);
		dst = asmstr + asmsz;
		src = tok.lit;
		if (*src != '"')
			error(&tok.loc, "__asm__ literals must not be wide strings");
		for (++src; *src != '"'; ++dst)
			*dst = unescape(&src);
		asmsz = dst - asmstr;
		next();
	} while (tok.kind == TSTRINGLIT);
	*dst = '\0';
	expect(TRPAREN, "after __asm__ text");
	expect(TSEMICOLON, "after __asm__");

	emittoplevelasm(asmstr, asmsz);
	free(asmstr);
}
