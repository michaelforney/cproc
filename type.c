#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "util.h"
#include "cc.h"

struct type typevoid    = {.kind = TYPEVOID, .incomplete = true};

struct type typechar    = {.kind = TYPECHAR, .size = 1, .align = 1, .repr = &i8, .basic.issigned = 1};
struct type typeschar   = {.kind = TYPECHAR, .size = 1, .align = 1, .repr = &i8, .basic.issigned = 1};
struct type typeuchar   = {.kind = TYPECHAR, .size = 1, .align = 1, .repr = &i8};

struct type typeshort   = {.kind = TYPESHORT, .size = 2, .align = 2, .repr = &i16, .basic.issigned = 1};
struct type typeushort  = {.kind = TYPESHORT, .size = 2, .align = 2, .repr = &i16};

struct type typeint     = {.kind = TYPEINT, .size = 4, .align = 4, .repr = &i32, .basic.issigned = 1};
struct type typeuint    = {.kind = TYPEINT, .size = 4, .align = 4, .repr = &i32};

struct type typelong    = {.kind = TYPELONG, .size = 8, .align = 8, .repr = &i64, .basic.issigned = 1};
struct type typeulong   = {.kind = TYPELONG, .size = 8, .align = 8, .repr = &i64};

struct type typellong   = {.kind = TYPELLONG, .size = 8, .align = 8, .repr = &i64, .basic.issigned = 1};
struct type typeullong  = {.kind = TYPELLONG, .size = 8, .align = 8, .repr = &i64};

struct type typebool    = {.kind = TYPEBOOL, .size = 1, .align = 1, .repr = &i8};
struct type typefloat   = {.kind = TYPEFLOAT, .size = 4, .align = 4, .repr = &f32};
struct type typedouble  = {.kind = TYPEDOUBLE, .size = 8, .align = 8, .repr = &f64};
struct type typeldouble = {.kind = TYPELDOUBLE, .size = 16, .align = 16};  // XXX: not supported by qbe

static struct type typevaliststruct = {.kind = TYPESTRUCT, .size = 24, .align = 8};
struct type typevalist = {.kind = TYPEARRAY, .size = 24, .align = 8, .array = {1}, .base = &typevaliststruct};
struct type typevalistptr = {.kind = TYPEPOINTER, .size = 8, .align = 8, .repr = &i64, .base = &typevaliststruct};

struct type *
mktype(enum typekind kind)
{
	struct type *t;

	t = xmalloc(sizeof(*t));
	t->kind = kind;
	t->incomplete = 0;

	return t;
}

struct type *
mkpointertype(struct type *base, enum typequal qual)
{
	struct type *t;

	t = mktype(TYPEPOINTER);
	t->base = base;
	t->qual = qual;
	t->size = 8;
	t->align = 8;
	t->repr = &i64;

	return t;
}

struct type *
mkarraytype(struct type *base, enum typequal qual, uint64_t len)
{
	struct type *t;

	t = mktype(TYPEARRAY);
	t->base = base;
	t->qual = qual;
	t->array.length = len;
	t->incomplete = !len;
	if (t->base) {
		t->align = t->base->align;
		t->size = t->base->size * len;  // XXX: overflow?
	}

	return t;
}

enum typeprop
typeprop(struct type *t)
{
	enum typeprop p;

	switch (t->kind) {
	case TYPEVOID:
		p = PROPOBJECT;
		break;
	case TYPECHAR:
		p = PROPOBJECT|PROPARITH|PROPSCALAR|PROPREAL|PROPINT|PROPCHAR;
		break;
	case TYPEBOOL:
	case TYPESHORT:
	case TYPEINT:
	case TYPEENUM:
	case TYPELONG:
	case TYPELLONG:
		p = PROPOBJECT|PROPARITH|PROPSCALAR|PROPREAL|PROPINT;
		break;
	case TYPEFLOAT:
	case TYPEDOUBLE:
	case TYPELDOUBLE:
		p = PROPOBJECT|PROPARITH|PROPSCALAR|PROPFLOAT;
		if (!t->basic.iscomplex)
			p |= PROPREAL;
		break;
	case TYPEPOINTER:
		p = PROPOBJECT|PROPSCALAR|PROPDERIVED;
		break;
	case TYPEARRAY:
		p = PROPOBJECT|PROPAGGR|PROPDERIVED;
		break;
	case TYPEFUNC:
		p = PROPDERIVED;
		break;
	case TYPESTRUCT:
		p = PROPOBJECT|PROPAGGR;
		break;
	case TYPEUNION:
		p = PROPOBJECT;
		break;
	default:
		fatal("unknown type");
	}

	return p;
}

static int
typerank(struct type *t)
{
	assert(typeprop(t) & PROPINT);
	switch (t->kind) {
	case TYPEBOOL:  return 1;
	case TYPECHAR:  return 2;
	case TYPESHORT: return 3;
	case TYPEENUM:
	case TYPEINT:   return 4;
	case TYPELONG:  return 5;
	case TYPELLONG: return 6;
	default:
		fatal("internal error; unhandled integer type");
	}
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
		       t1->basic.issigned == t2->basic.issigned;
	}
	switch (t1->kind) {
	case TYPEVOID:
		return true;
	case TYPEPOINTER:
		goto derived;
	case TYPEARRAY:
		if (t1->array.length && t2->array.length && t1->array.length != t2->array.length)
			return false;
		goto derived;
	case TYPEFUNC:
		if (!t1->func.isprototype) {
			if (!t2->func.isprototype)
				return true;
			tmp = t1, t1 = t2, t2 = tmp;
		}
		if (t1->func.isvararg != t2->func.isvararg)
			return false;
		if (!t2->func.paraminfo) {
			for (p1 = t1->func.params; p1; p1 = p1->next) {
				if (!typecompatible(p1->type, typeargpromote(p1->type)))
					return false;
			}
			return true;
		}
		for (p1 = t1->func.params, p2 = t2->func.params; p1 && p2; p1 = p1->next, p2 = p2->next) {
			if (p1->qual != p2->qual)
				return false;
			tmp = t2->func.isprototype ? p2->type : typeargpromote(p2->type);
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
	// XXX: implement
	return typecompatible(t1, t2);
}

struct type *
typecomposite(struct type *t1, struct type *t2)
{
	// XXX: implement 6.2.7
	// XXX: merge with typecompatible?
	return t1;
}

struct type *
typeintpromote(struct type *t)
{
	if (typeprop(t) & PROPINT && typerank(t) <= typerank(&typeint))
		return t->size < typeint.size || t->basic.issigned ? &typeint : &typeuint;
	return t;
}

struct type *
typeargpromote(struct type *t)
{
	if (t == &typefloat)
		return &typedouble;
	return typeintpromote(t);
}

struct type *
typecommonreal(struct type *t1, struct type *t2)
{
	struct type *tmp;

	assert(typeprop(t1) & PROPREAL && typeprop(t2) & PROPREAL);
	if (t1 == t2)
		return t1;
	if (t1 == &typeldouble || t2 == &typeldouble)
		return &typeldouble;
	if (t1 == &typedouble || t2 == &typedouble)
		return &typedouble;
	if (t1 == &typefloat || t2 == &typefloat)
		return &typefloat;
	t1 = typeintpromote(t1);
	t2 = typeintpromote(t2);
	if (t1 == t2)
		return t1;
	if (t1->basic.issigned == t2->basic.issigned)
		return typerank(t1) > typerank(t2) ? t1 : t2;
	if (t1->basic.issigned) {
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
}

struct member *
typemember(struct type *t, const char *name, uint64_t *offset)
{
	struct member *m, *sub;

	assert(t->kind == TYPESTRUCT || t->kind == TYPEUNION);
	for (m = t->structunion.members; m; m = m->next) {
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
