#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "util.h"
#include "cc.h"

#define INTTYPE(k, n, s, p) { \
	.kind = k, .size = n, .align = n, .u.basic.issigned = s, \
	.prop = PROPSCALAR|PROPARITH|PROPREAL|PROPINT|p, \
}
#define FLTTYPE(k, n) { \
	.kind = k, .size = n, .align = n, \
	.prop = PROPSCALAR|PROPARITH|PROPREAL|PROPFLOAT, \
}

struct type typevoid    = {.kind = TYPEVOID, .incomplete = true};

struct type typebool    = INTTYPE(TYPEBOOL, 1, false, 0);

struct type typechar    = INTTYPE(TYPECHAR, 1, true, PROPCHAR);
struct type typeschar   = INTTYPE(TYPECHAR, 1, true, PROPCHAR);
struct type typeuchar   = INTTYPE(TYPECHAR, 1, false, PROPCHAR);

struct type typeshort   = INTTYPE(TYPESHORT, 2, true, 0);
struct type typeushort  = INTTYPE(TYPESHORT, 2, false, 0);

struct type typeint     = INTTYPE(TYPEINT, 4, true, 0);
struct type typeuint    = INTTYPE(TYPEINT, 4, false, 0);

struct type typelong    = INTTYPE(TYPELONG, 8, true, 0);
struct type typeulong   = INTTYPE(TYPELONG, 8, false, 0);

struct type typellong   = INTTYPE(TYPELLONG, 8, true, 0);
struct type typeullong  = INTTYPE(TYPELLONG, 8, false, 0);

struct type typefloat   = FLTTYPE(TYPEFLOAT, 4);
struct type typedouble  = FLTTYPE(TYPEDOUBLE, 8);
struct type typeldouble = FLTTYPE(TYPELDOUBLE, 16);

struct type *typeadjvalist;

struct type *
mktype(enum typekind kind, enum typeprop prop)
{
	struct type *t;

	t = xmalloc(sizeof(*t));
	t->kind = kind;
	t->prop = prop;
	t->value = NULL;
	t->incomplete = false;
	t->flexible = false;

	return t;
}

struct type *
mkpointertype(struct type *base, enum typequal qual)
{
	struct type *t;

	t = mktype(TYPEPOINTER, PROPSCALAR);
	t->base = base;
	t->qual = qual;
	t->size = 8;
	t->align = 8;

	return t;
}

struct type *
mkarraytype(struct type *base, enum typequal qual, unsigned long long len)
{
	struct type *t;

	t = mktype(TYPEARRAY, 0);
	t->base = base;
	t->qual = qual;
	t->u.array.length = len;
	t->incomplete = !len;
	if (t->base) {
		t->align = t->base->align;
		t->size = t->base->size * len;  /* XXX: overflow? */
	}

	return t;
}

static int
typerank(struct type *t)
{
	assert(t->prop & PROPINT);
	switch (t->kind) {
	case TYPEBOOL:  return 1;
	case TYPECHAR:  return 2;
	case TYPESHORT: return 3;
	case TYPEENUM:
	case TYPEINT:   return 4;
	case TYPELONG:  return 5;
	case TYPELLONG: return 6;
	}
	fatal("internal error; unhandled integer type");
	return 0;
}

bool
typecompatible(struct type *t1, struct type *t2)
{
	struct type *tmp;
	struct param *p1, *p2;

	if (t1 == t2)
		return true;
	if (t1->kind != t2->kind) {
		/* enum types are compatible with 'int', but not with
		   each other (unless they are the same type) */
		return (t1->kind == TYPEENUM && t2->kind == TYPEINT ||
		        t1->kind == TYPEINT && t2->kind == TYPEENUM) &&
		       t1->u.basic.issigned == t2->u.basic.issigned;
	}
	switch (t1->kind) {
	case TYPEPOINTER:
		goto derived;
	case TYPEARRAY:
		if (t1->u.array.length && t2->u.array.length && t1->u.array.length != t2->u.array.length)
			return false;
		goto derived;
	case TYPEFUNC:
		if (!t1->u.func.isprototype) {
			if (!t2->u.func.isprototype)
				return true;
			tmp = t1, t1 = t2, t2 = tmp;
		}
		if (t1->u.func.isvararg != t2->u.func.isvararg)
			return false;
		if (!t2->u.func.paraminfo) {
			for (p1 = t1->u.func.params; p1; p1 = p1->next) {
				if (!typecompatible(p1->type, typepromote(p1->type, -1)))
					return false;
			}
			return true;
		}
		for (p1 = t1->u.func.params, p2 = t2->u.func.params; p1 && p2; p1 = p1->next, p2 = p2->next) {
			tmp = t2->u.func.isprototype ? p2->type : typepromote(p2->type, -1);
			if (!typecompatible(p1->type, tmp))
				return false;
		}
		if (p1 || p2)
			return false;
		goto derived;
	derived:
		return t1->qual == t2->qual && typecompatible(t1->base, t2->base);
	}
	return false;
}

bool
typesame(struct type *t1, struct type *t2)
{
	/* XXX: implement */
	return typecompatible(t1, t2);
}

struct type *
typecomposite(struct type *t1, struct type *t2)
{
	/* XXX: implement 6.2.7 */
	/* XXX: merge with typecompatible? */
	return t1;
}

struct type *
typepromote(struct type *t, unsigned width)
{
	if (t == &typefloat)
		return &typedouble;
	if (t->prop & PROPINT && (typerank(t) <= typerank(&typeint) || width <= typeint.size * 8)) {
		if (width == -1)
			width = t->size * 8;
		return width - t->u.basic.issigned < typeint.size * 8 ? &typeint : &typeuint;
	}
	return t;
}

struct type *
typecommonreal(struct type *t1, unsigned w1, struct type *t2, unsigned w2)
{
	struct type *tmp;

	assert(t1->prop & PROPREAL && t2->prop & PROPREAL);
	if (t1 == &typeldouble || t2 == &typeldouble)
		return &typeldouble;
	if (t1 == &typedouble || t2 == &typedouble)
		return &typedouble;
	if (t1 == &typefloat || t2 == &typefloat)
		return &typefloat;
	t1 = typepromote(t1, w1);
	t2 = typepromote(t2, w2);
	if (t1 == t2)
		return t1;
	if (t1->u.basic.issigned == t2->u.basic.issigned)
		return typerank(t1) > typerank(t2) ? t1 : t2;
	if (t1->u.basic.issigned) {
		tmp = t1;
		t1 = t2;
		t2 = tmp;
	}
	if (typerank(t1) >= typerank(t2))
		return t1;
	if (t1->size < t2->size)
		return t2;
	if (t2 == &typelong)
		return &typeulong;
	if (t2 == &typellong)
		return &typeullong;
	fatal("internal error; could not find common real type");
	return NULL;
}

/* function parameter type adjustment (C11 6.7.6.3p7) */
struct type *
typeadjust(struct type *t)
{
	switch (t->kind) {
	case TYPEARRAY:
		t = mkpointertype(t->base, t->qual);
		break;
	case TYPEFUNC:
		t = mkpointertype(t, QUALNONE);
		break;
	}

	return t;
}

struct member *
typemember(struct type *t, const char *name, unsigned long long *offset)
{
	struct member *m, *sub;

	assert(t->kind == TYPESTRUCT || t->kind == TYPEUNION);
	for (m = t->u.structunion.members; m; m = m->next) {
		if (m->name) {
			if (strcmp(m->name, name) == 0) {
				*offset += m->offset;
				return m;
			}
		} else {
			sub = typemember(m->type, name, offset);
			if (sub) {
				*offset += m->offset;
				return sub;
			}
		}
	}
	return NULL;
}

struct param *
mkparam(char *name, struct type *t, enum typequal tq)
{
	struct param *p;

	p = xmalloc(sizeof(*p));
	p->name = name;
	p->type = t;
	p->qual = tq;
	p->next = NULL;

	return p;
}
