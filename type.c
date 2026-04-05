#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "util.h"
#include "cc.h"

#define INTTYPE(k, n, s, p) { \
	.kind = k, .size = n, .align = n, \
	.u.arith = {.issigned = s, .width = n * 8}, \
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

struct type typenullptr = {.kind = TYPENULLPTR, .size = 8, .align = 8, .prop = PROPSCALAR};

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
	if (base)
		t->prop |= base->prop & PROPVM;

	return t;
}

struct type *
mkarraytype(struct type *base, enum typequal qual, unsigned long long len)
{
	struct type *t;

	t = mktype(TYPEARRAY, 0);
	t->base = base;
	t->qual = qual;
	t->u.array.length = NULL;
	t->u.array.ptrqual = QUALNONE;
	t->incomplete = !len;
	if (t->base) {
		t->align = t->base->align;
		t->size = t->base->size * len;  /* XXX: overflow? */
	}

	return t;
}

struct type *
mkbitinttype(int width, bool sign)
{
	struct type *t;

	t = mktype(TYPEBITINT, PROPSCALAR | PROPARITH | PROPREAL | PROPINT);
	t->u.arith.issigned = sign;
	t->u.arith.width = width;
	if (width < 1 + sign || width > 64)
		error(&tok.loc, "invalid %s _BitInt width %d", sign ? "signed" : "unsigned", width);

	/* calculate byte size */
	t->size = 1;
	while (t->size * 8 < width)
		t->size <<= 1;
	t->align = t->size;

	return t;
}

/*
We define type rank using the number of bits in the type shifted
left by 4. The least significant 4 bits are used to establish a
ranking between integer types with the same width (such as long and
long long on 64-bit platforms).
*/
static int
typerank(struct type *t)
{
	if (t->kind == TYPEENUM)
		t = t->base;
	assert(t->prop & PROPINT);
	switch (t->kind) {
	case TYPEBOOL:   return 0;
	case TYPECHAR:   return 0x081;
	case TYPESHORT:  return 0x101;
	case TYPEINT:    return 0x201;
	case TYPELONG:   return t->size << 3 + 4 | 0x1;
	case TYPELLONG:  return 0x402;
	case TYPEBITINT: return t->u.arith.width << 4;
	}
	fatal("internal error; unhandled integer type");
	return -1;
}

bool
typecompatible(struct type *t1, struct type *t2)
{
	struct decl *p1, *p2;
	struct expr *e1, *e2;

	if (t1 == t2)
		return true;
	if (t1->kind != t2->kind) {
		/*
		enum types are compatible with their underlying
		type, but not with each other (unless they are the
		same type)
		*/
		return t1->kind == TYPEENUM && t2 == t1->base ||
		       t2->kind == TYPEENUM && t1 == t2->base;
	}
	switch (t1->kind) {
	case TYPEBITINT:
		return t1->u.arith.width == t2->u.arith.width && t1->u.arith.issigned == t2->u.arith.issigned;
	case TYPEPOINTER:
		goto derived;
	case TYPEARRAY:
		if (t1->incomplete || t2->incomplete)
			goto derived;
		e1 = t1->u.array.length;
		e2 = t2->u.array.length;
		if (e1 && e2 && e1->kind == EXPRCONST && e2->kind == EXPRCONST && e1->u.constant.u != e2->u.constant.u)
			return false;
		goto derived;
	case TYPEFUNC:
		if (t1->u.func.isvararg != t2->u.func.isvararg)
			return false;
		for (p1 = t1->u.func.params, p2 = t2->u.func.params; p1 && p2; p1 = p1->next, p2 = p2->next) {
			if (!typecompatible(p1->type, p2->type))
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
	if (t->kind == TYPEBITINT)
		return t;
	if (t->prop & PROPINT && (typerank(t) <= typerank(&typeint) || width <= typeint.size * 8)) {
		if (width == -1)
			width = t->size * 8;
		return width - t->u.arith.issigned < typeint.size * 8 ? &typeint : &typeuint;
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
	if (t1->u.arith.issigned == t2->u.arith.issigned)
		return typerank(t1) > typerank(t2) ? t1 : t2;
	if (t1->u.arith.issigned) {
		tmp = t1;
		t1 = t2;
		t2 = tmp;
	}
	/* t1 is unsigned and t2 is signed */
	if (typerank(t1) >= typerank(t2))
		return t1;
	/* t1 has a lower rank than t2 */
	if (t1->u.arith.width < t2->u.arith.width)
		return t2;
	switch (t2->kind) {
	case TYPEINT: return &typeuint;
	case TYPELONG: return &typeulong;
	case TYPELLONG: return &typellong;
	}
	fatal("internal error; could not find common real type");
	return NULL;
}

/* function parameter type adjustment (C11 6.7.6.3p7) */
struct type *
typeadjust(struct type *t, enum typequal *tq)
{
	enum typequal ptrqual;

	switch (t->kind) {
	case TYPEARRAY:
		ptrqual = t->u.array.ptrqual;
		t = mkpointertype(t->base, *tq | t->qual);
		*tq = ptrqual;
		break;
	case TYPEFUNC:
		assert(*tq == QUALNONE);
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

bool
typehasint(struct type *t, unsigned long long i, bool sign)
{
	assert(t->prop & PROPINT);
	if (sign && i >= -1ull << 63)
		return t->u.arith.issigned && i >= -1ull << (t->size << 3) - 1;
	return i <= 0xffffffffffffffffull >> (8 - t->size << 3) + t->u.arith.issigned;
}
