#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "util.h"
#include "cc.h"

struct scope filescope;

void
scopeinit(void)
{
	static struct builtin {
		char *name;
		struct decl decl;
	} builtins[] = {
		{"__builtin_alloca",     {.kind = DECLBUILTIN, .u.builtin = BUILTINALLOCA}},
		{"__builtin_constant_p", {.kind = DECLBUILTIN, .u.builtin = BUILTINCONSTANTP}},
		{"__builtin_expect",     {.kind = DECLBUILTIN, .u.builtin = BUILTINEXPECT}},
		{"__builtin_inff",       {.kind = DECLBUILTIN, .u.builtin = BUILTININFF}},
		{"__builtin_nanf",       {.kind = DECLBUILTIN, .u.builtin = BUILTINNANF}},
		{"__builtin_offsetof",   {.kind = DECLBUILTIN, .u.builtin = BUILTINOFFSETOF}},
		{"__builtin_types_compatible_p",
			{.kind = DECLBUILTIN, .u.builtin = BUILTINTYPESCOMPATIBLEP}},
		{"__builtin_unreachable", {.kind = DECLBUILTIN, .u.builtin = BUILTINUNREACHABLE}},
		{"__builtin_va_arg",     {.kind = DECLBUILTIN, .u.builtin = BUILTINVAARG}},
		{"__builtin_va_copy",    {.kind = DECLBUILTIN, .u.builtin = BUILTINVACOPY}},
		{"__builtin_va_end",     {.kind = DECLBUILTIN, .u.builtin = BUILTINVAEND}},
		{"__builtin_va_start",   {.kind = DECLBUILTIN, .u.builtin = BUILTINVASTART}},
	};
	static struct decl valist;
	struct builtin *b;

	for (b = builtins; b < builtins + LEN(builtins); ++b)
		scopeputdecl(&filescope, b->name, &b->decl);
	valist.kind = DECLTYPE;
	valist.type = targ->typevalist;
	scopeputdecl(&filescope, "__builtin_va_list", &valist);
}

struct scope *
mkscope(struct scope *parent)
{
	struct scope *s;

	s = xmalloc(sizeof(*s));
	s->decls = NULL;
	s->tags = NULL;
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

	if (s->decls)
		delmap(s->decls, NULL);
	if (s->tags)
		delmap(s->tags, NULL);
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
		d = s->decls ? mapget(s->decls, &k) : NULL;
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
		t = s->tags ? mapget(s->tags, &k) : NULL;
		s = s->parent;
	} while (!t && s && recurse);

	return t;
}

void
scopeputdecl(struct scope *s, const char *name, struct decl *d)
{
	struct mapkey k;

	if (!s->decls)
		s->decls = mkmap(32);
	mapkey(&k, name, strlen(name));
	*mapput(s->decls, &k) = d;
}

void
scopeputtag(struct scope *s, const char *name, struct type *t)
{
	struct mapkey k;

	if (!s->tags)
		s->tags = mkmap(32);
	mapkey(&k, name, strlen(name));
	*mapput(s->tags, &k) = t;
}
