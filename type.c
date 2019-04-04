#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "util.h"
#include "backend.h"
#include "type.h"

struct type typevoid       = {.kind = TYPEVOID, .incomplete = true};

struct type typechar       = {.kind = TYPEBASIC, .size = 1, .align = 1, .repr = &i8, .basic = {.kind = BASICCHAR, .issigned = 1}};
struct type typeschar      = {.kind = TYPEBASIC, .size = 1, .align = 1, .repr = &i8, .basic = {.kind = BASICCHAR, .issigned = 1}};
struct type typeuchar      = {.kind = TYPEBASIC, .size = 1, .align = 1, .repr = &i8, .basic = {.kind = BASICCHAR}};

struct type typeshort      = {.kind = TYPEBASIC, .size = 2, .align = 2, .repr = &i16, .basic = {.kind = BASICSHORT, .issigned = 1}};
struct type typeushort     = {.kind = TYPEBASIC, .size = 2, .align = 2, .repr = &i16, .basic = {.kind = BASICSHORT}};

struct type typeint        = {.kind = TYPEBASIC, .size = 4, .align = 4, .repr = &i32, .basic = {.kind = BASICINT, .issigned = 1}};
struct type typeuint       = {.kind = TYPEBASIC, .size = 4, .align = 4, .repr = &i32, .basic = {.kind = BASICINT}};

struct type typelong       = {.kind = TYPEBASIC, .size = 8, .align = 8, .repr = &i64, .basic = {.kind = BASICLONG, .issigned = 1}};
struct type typeulong      = {.kind = TYPEBASIC, .size = 8, .align = 8, .repr = &i64, .basic = {.kind = BASICLONG}};

struct type typellong      = {.kind = TYPEBASIC, .size = 8, .align = 8, .repr = &i64, .basic = {.kind = BASICLONGLONG, .issigned = 1}};
struct type typeullong     = {.kind = TYPEBASIC, .size = 8, .align = 8, .repr = &i64, .basic = {.kind = BASICLONGLONG}};

struct type typebool       = {.kind = TYPEBASIC, .size = 1, .align = 1, .repr = &i8, .basic = {.kind = BASICBOOL}};
struct type typefloat      = {.kind = TYPEBASIC, .size = 4, .align = 4, .repr = &f32, .basic = {.kind = BASICFLOAT}};
struct type typedouble     = {.kind = TYPEBASIC, .size = 8, .align = 8, .repr = &f64, .basic = {.kind = BASICDOUBLE}};
struct type typelongdouble = {.kind = TYPEBASIC, .size = 16, .align = 16, .basic = {.kind = BASICLONGDOUBLE}};  // XXX: not supported by qbe

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
mkqualifiedtype(struct type *base, enum typequal tq)
{
	struct type *t;

	if (!tq)
		return base;
	t = mktype(TYPEQUALIFIED);
	t->base = base;
	t->qualified.kind = tq;
	if (base) {
		t->size = base->size;
		t->align = base->align;
		t->repr = base->repr;
	}
	// XXX: incomplete?
	return t;
}

struct type *
mkpointertype(struct type *base)
{
	struct type *t;

	t = mktype(TYPEPOINTER);
	t->base = base;
	t->size = 8;
	t->align = 8;
	t->repr = &i64;

	return t;
}

struct type *
mkarraytype(struct type *base, uint64_t len)
{
	struct type *t;

	t = mktype(TYPEARRAY);
	t->base = base;
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
	enum typeprop p = PROPNONE;

	switch (t->kind) {
	case TYPEVOID:
		p |= PROPOBJECT;
		break;
	case TYPEBASIC:
		p |= PROPOBJECT|PROPARITH|PROPSCALAR;
		if (!t->basic.iscomplex)
			p |= PROPREAL;
		switch (t->basic.kind) {
		case BASICFLOAT:
		case BASICDOUBLE:
		case BASICLONGDOUBLE:
			p |= PROPFLOAT;
			break;
		case BASICCHAR:
			p |= PROPCHAR;
			/* fallthrough */
		default:
			p |= PROPINT;
			break;
		}
		break;
	case TYPEPOINTER:
		p |= PROPOBJECT|PROPSCALAR|PROPDERIVED;
		break;
	case TYPEARRAY:
		p |= PROPOBJECT|PROPAGGR|PROPDERIVED;
		break;
	case TYPEFUNC:
		p |= PROPDERIVED;
		break;
	case TYPESTRUCT:
		p |= PROPOBJECT|PROPAGGR;
		break;
	default:
		break;
	}

	return p;
}

static int
typerank(struct type *t)
{
	assert(typeprop(t) & PROPINT);
	switch (t->basic.kind) {
	case BASICBOOL:     return 1;
	case BASICCHAR:     return 2;
	case BASICSHORT:    return 3;
	case BASICENUM:
	case BASICINT:      return 4;
	case BASICLONG:     return 5;
	case BASICLONGLONG: return 6;
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
	if (t1->kind != t2->kind)
		return false;
	switch (t1->kind) {
	case TYPEBASIC:
		if (t1->basic.issigned != t2->basic.issigned)
			return false;
		/* enum types are compatible with 'int', but not with
		   each other (unless they are the same type) */
		return t1->basic.kind == BASICENUM && t2->basic.kind == BASICINT ||
		       t1->basic.kind == BASICINT && t2->basic.kind == BASICENUM;
	case TYPEQUALIFIED:
		if (t1->qualified.kind != t2->qualified.kind)
			return false;
		return typecompatible(t1->base, t2->base);
	case TYPEVOID:
		return true;
	case TYPEPOINTER:
		return typecompatible(t1->base, t2->base);
	case TYPEARRAY:
		if (t1->array.length && t2->array.length && t1->array.length != t2->array.length)
			return false;
		return typecompatible(t1->base, t2->base);
	case TYPEFUNC:
		if (!typecompatible(t1->base, t2->base))
			return false;
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
			tmp = t2->func.isprototype ? p2->type : typeargpromote(p2->type);
			if (!typecompatible(p1->type, tmp))
				return false;
		}
		return !p1 && !p2;
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
typeunqual(struct type *t, enum typequal *tq)
{
	while (t->kind == TYPEQUALIFIED) {
		if (tq)
			*tq |= t->qualified.kind;
		t = t->base;
	}
	return t;
}

struct type *
typeintpromote(struct type *t)
{
	assert(t->kind != TYPEQUALIFIED);
	if (typeprop(t) & PROPINT && typerank(t) <= typerank(&typeint))
		return t->size < typeint.size || t->basic.issigned ? &typeint : &typeuint;
	return t;
}

struct type *
typeargpromote(struct type *t)
{
	assert(t->kind != TYPEQUALIFIED);
	if (t == &typefloat)
		return &typedouble;
	return typeintpromote(t);
}

struct type *
typecommonreal(struct type *t1, struct type *t2)
{
	struct type *tmp;

	assert(t1->kind == TYPEBASIC && t2->kind == TYPEBASIC);
	if (t1 == t2)
		return t1;
	if (t1 == &typelongdouble || t2 == &typelongdouble)
		return &typelongdouble;
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
mkparam(char *name, struct type *t)
{
	struct param *p;

	p = xmalloc(sizeof(*p));
	p->name = name;
	p->type = t;
	p->next = NULL;

	return p;
}
