#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "cc.h"

enum attrprefix {
	PREFIXNONE = 1,  /* standard attribute */
	PREFIXGNU,
};

static char *
strip(char *name)
{
	size_t len;

	len = strlen(name);
	if (len >= 4 && name[0] == '_' && name[1] == '_' && name[len - 2] == '_' && name[len - 1] == '_') {
		name[len - 2] = '\0';
		name += 2;
	}
	return name;
}

static bool
parseattr(struct attr *a, enum attrkind allowed, enum attrprefix prefix)
{
	char *name, *prefixname = "";
	enum attrkind kind;
	int paren;

	if (tok.kind != TIDENT)
		return false;
	name = strip(tok.lit);
	next();
	if (!prefix) {
		if (consume(TCOLONCOLON)) {
			if (strcmp(name, "gnu") == 0)
				prefix = PREFIXGNU;
			else
				prefix = 0;
			name = strip(expect(TIDENT, "after attribute prefix"));
		} else {
			prefix = PREFIXNONE;
		}
	}
	kind = 0;
	switch (prefix) {
	case PREFIXGNU:
		prefixname = "GNU ";
		if (strcmp(name, "aligned") == 0) {
			kind = ATTRALIGNED;
			if (consume(TLPAREN)) {
				unsigned long long i;

				i = intconstexpr(&filescope, false);
				if (!i || i & i - 1 || i > INT_MAX)
					error(&tok.loc, "invalid alignment %llu", i);
				if (a)
					a->align = i;
				expect(TRPAREN, "after alignment");
			} else {
				if (a)
					a->align = 16;
			}
		} else if (strcmp(name, "constructor") == 0) {
			kind = ATTRCONSTRUCTOR;
		} else if (strcmp(name, "destructor") == 0) {
			kind = ATTRDESTRUCTOR;
		} else if (strcmp(name, "packed") == 0) {
			kind = ATTRPACKED;
		}
		break;
	}
	if (kind) {
		if (!(kind & allowed))
			error(&tok.loc, "%sattribute '%s' is not supported here", prefixname, name);
		if (a)
			a->kind |= kind;
	} else if (consume(TLPAREN)) {
		/* skip arguments */
		for (paren = 1; paren > 0; next()) {
			switch (tok.kind) {
			case TLPAREN: ++paren; break;
			case TRPAREN: --paren; break;
			}
		}
	}
	return true;
}

static bool
attrspec(struct attr *a, enum attrkind allowed)
{
	if (tok.kind != TLBRACK || !peek(TLBRACK))
		return false;
	while (parseattr(a, allowed, 0) || consume(TCOMMA))
		;
	expect(TRBRACK, "to end attribute specifier");
	expect(TRBRACK, "to end attribute specifier");
	return true;
}

bool
attr(struct attr *a, enum attrkind allowed)
{
	if (!attrspec(a, allowed))
		return false;
	while (attrspec(a, allowed))
		;
	return true;
}

static bool
gnuattrspec(struct attr *a, enum attrkind allowed)
{
	if (!consume(T__ATTRIBUTE__))
		return false;
	while (parseattr(a, allowed, PREFIXGNU) || consume(TCOMMA))
		;
	expect(TLPAREN, "after '__attribute__' to begin attribute specifier");
	expect(TLPAREN, "after '__attribute__' to begin attribute specifier");
	while (parseattr(a, allowed, PREFIXGNU) || consume(TCOMMA))
		;
	expect(TRPAREN, "to end attribute specifier");
	expect(TRPAREN, "to end attribute specifier");
	return true;
}

bool
gnuattr(struct attr *a, enum attrkind allowed)
{
	if (!gnuattrspec(a, allowed))
		return false;
	while (gnuattrspec(a, allowed))
		;
	return true;
}
