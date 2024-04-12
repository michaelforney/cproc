#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "cc.h"

struct scope filescope;

void
scopeinit(void)
{
	static struct decl builtins[] = {
		{.name = "__builtin_alloca",      .kind = DECLBUILTIN, .u.builtin = BUILTINALLOCA},
		{.name = "__builtin_constant_p",  .kind = DECLBUILTIN, .u.builtin = BUILTINCONSTANTP},
		{.name = "__builtin_expect",      .kind = DECLBUILTIN, .u.builtin = BUILTINEXPECT},
		{.name = "__builtin_inff",        .kind = DECLBUILTIN, .u.builtin = BUILTININFF},
		{.name = "__builtin_nanf",        .kind = DECLBUILTIN, .u.builtin = BUILTINNANF},
		{.name = "__builtin_offsetof",    .kind = DECLBUILTIN, .u.builtin = BUILTINOFFSETOF},
		{.name = "__builtin_types_compatible_p", .kind = DECLBUILTIN, .u.builtin = BUILTINTYPESCOMPATIBLEP},
		{.name = "__builtin_unreachable", .kind = DECLBUILTIN, .u.builtin = BUILTINUNREACHABLE},
		{.name = "__builtin_va_arg",      .kind = DECLBUILTIN, .u.builtin = BUILTINVAARG},
		{.name = "__builtin_va_copy",     .kind = DECLBUILTIN, .u.builtin = BUILTINVACOPY},
		{.name = "__builtin_va_end",      .kind = DECLBUILTIN, .u.builtin = BUILTINVAEND},
		{.name = "__builtin_va_start",    .kind = DECLBUILTIN, .u.builtin = BUILTINVASTART},
	};
	static struct decl valist;
	struct decl *d;

	for (d = builtins; d < builtins + LEN(builtins); ++d)
		scopeputdecl(&filescope, d);
	valist.name = "__builtin_va_list";
	valist.kind = DECLTYPE;
	valist.type = targ->typevalist;
	scopeputdecl(&filescope, &valist);
}

struct scope *
mkscope(struct scope *parent)
{
	struct scope *s;

	s = xmalloc(sizeof(*s));
	s->decls.len = 0;
	s->tags.len = 0;
	s->breaklabel = parent->breaklabel;
	s->continuelabel = parent->continuelabel;
	s->switchcases = parent->switchcases;
	s->parent = parent;

	return s;
}

struct scope *
delscope(struct scope *s)
{
	struct scope *parent = s->parent;

	if (s->decls.len)
		mapfree(&s->decls, NULL);
	if (s->tags.len)
		mapfree(&s->tags, NULL);
	free(s);

	return parent;
}

struct decl *
scopegetdecl(struct scope *s, const char *name, bool recurse)
{
	struct decl *d;
	struct mapkey k;

	mapkey(&k, name, strlen(name));
	do {
		d = s->decls.len ? mapget(&s->decls, &k) : NULL;
		s = s->parent;
	} while (!d && s && recurse);

	return d;
}

struct type *
scopegettag(struct scope *s, const char *name, bool recurse)
{
	struct type *t;
	struct mapkey k;

	mapkey(&k, name, strlen(name));
	do {
		t = s->tags.len ? mapget(&s->tags, &k) : NULL;
		s = s->parent;
	} while (!t && s && recurse);

	return t;
}

void
scopeputdecl(struct scope *s, struct decl *d)
{
	struct mapkey k;

	if (!s->decls.len)
		mapinit(&s->decls, 32);
	mapkey(&k, d->name, strlen(d->name));
	*mapput(&s->decls, &k) = d;
}

void
scopeputtag(struct scope *s, const char *name, struct type *t)
{
	struct mapkey k;

	if (!s->tags.len)
		mapinit(&s->tags, 32);
	mapkey(&k, name, strlen(name));
	*mapput(&s->tags, &k) = t;
}
