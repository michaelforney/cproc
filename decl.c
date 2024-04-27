#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "cc.h"

static struct decl *tentativedefns, **tentativedefnsend = &tentativedefns;

struct qualtype {
	struct type *type;
	enum typequal qual;
	struct expr *expr;
};

enum storageclass {
	SCNONE,

	SCTYPEDEF     = 1<<1,
	SCEXTERN      = 1<<2,
	SCSTATIC      = 1<<3,
	SCAUTO        = 1<<4,
	SCREGISTER    = 1<<5,
	SCTHREADLOCAL = 1<<6,
};

enum typespec {
	SPECNONE,

	SPECVOID     = 1<<1,
	SPECCHAR     = 1<<2,
	SPECBOOL     = 1<<3,
	SPECINT      = 1<<4,
	SPECFLOAT    = 1<<5,
	SPECDOUBLE   = 1<<6,
	SPECSHORT    = 1<<7,
	SPECLONG     = 1<<8,
	SPECLONG2    = 1<<9,
	SPECSIGNED   = 1<<10,
	SPECUNSIGNED = 1<<11,
	SPECCOMPLEX  = 1<<12,

	SPECLONGLONG = SPECLONG|SPECLONG2,
};

enum funcspec {
	FUNCNONE,

	FUNCINLINE   = 1<<1,
	FUNCNORETURN = 1<<2,
};

struct structbuilder {
	struct type *type;
	struct member **last;
	unsigned bits;  /* number of bits remaining in the last byte */
	bool pack;
};

struct decl *
mkdecl(char *name, enum declkind k, struct type *t, enum typequal tq, enum linkage linkage)
{
	struct decl *d;

	d = xmalloc(sizeof(*d));
	memset(d, 0, sizeof(*d));
	d->name = name;
	d->kind = k;
	d->linkage = linkage;
	d->type = t;
	d->qual = tq;
	if (k == DECLOBJECT && t)
		d->u.obj.align = t->align;

	return d;
}

/* 6.7.1 Storage-class specifiers */
static int
storageclass(enum storageclass *sc)
{
	enum storageclass allowed, new;

	switch (tok.kind) {
	case TTYPEDEF:       new = SCTYPEDEF;     break;
	case TEXTERN:        new = SCEXTERN;      break;
	case TSTATIC:        new = SCSTATIC;      break;
	case TTHREAD_LOCAL:  new = SCTHREADLOCAL; break;
	case TAUTO:          new = SCAUTO;        break;
	case TREGISTER:      new = SCREGISTER;    break;
	default: return 0;
	}
	if (!sc)
		error(&tok.loc, "storage class not allowed in this declaration");
	switch (*sc) {
	case SCNONE:        allowed = ~SCNONE;           break;
	case SCTHREADLOCAL: allowed = SCSTATIC|SCEXTERN; break;
	case SCSTATIC:
	case SCEXTERN:      allowed = SCTHREADLOCAL;     break;
	default:            allowed = SCNONE;            break;
	}
	if (new & ~allowed)
		error(&tok.loc, "invalid combination of storage class specifiers");
	*sc |= new;
	next();

	return 1;
}

/* 6.7.3 Type qualifiers */
static int
typequal(enum typequal *tq)
{
	switch (tok.kind) {
	case TCONST:    *tq |= QUALCONST;    break;
	case TVOLATILE: *tq |= QUALVOLATILE; break;
	case TRESTRICT: *tq |= QUALRESTRICT; break;
	case T_ATOMIC: error(&tok.loc, "_Atomic type qualifier is not yet supported");
	default: return 0;
	}
	next();

	return 1;
}

/* 6.7.4 Function specifiers */
static int
funcspec(enum funcspec *fs)
{
	enum funcspec new;

	switch (tok.kind) {
	case TINLINE:    new = FUNCINLINE;   break;
	case T_NORETURN: new = FUNCNORETURN; break;
	default: return 0;
	}
	if (!fs)
		error(&tok.loc, "function specifier not allowed in this declaration");
	*fs |= new;
	next();

	return 1;
}

static void structdecl(struct scope *, struct structbuilder *);
static struct qualtype declspecs(struct scope *, enum storageclass *, enum funcspec *, int *);

static struct type *
tagspec(struct scope *s)
{
	static struct type *const inttypes[][2] = {
		{&typeuint, &typeint},
		{&typeulong, &typelong},
		{&typeullong, &typellong},
	};
	struct type *t, *et;
	char *tag, *name;
	enum typekind kind;
	struct decl *d, *enumconsts;
	struct expr *e;
	struct attr a;
	enum attrkind allowedattr;
	struct structbuilder b;
	unsigned long long value, max, min;
	bool sign;
	int i;

	allowedattr = 0;
	switch (tok.kind) {
	case TSTRUCT: kind = TYPESTRUCT, allowedattr |= ATTRPACKED; break;
	case TUNION:  kind = TYPEUNION; break;
	case TENUM:   kind = TYPEENUM; break;
	default: fatal("internal error: unknown tag kind");
	}
	next();
	a.kind = 0;
	attr(&a, allowedattr);
	gnuattr(&a, allowedattr);
	tag = NULL;
	t = NULL;
	et = NULL;
	if (tok.kind == TIDENT) {
		tag = tok.lit;
		next();
	}
	if (kind == TYPEENUM && consume(TCOLON)) {
		et = declspecs(s, NULL, NULL, NULL).type;
		if (!et)
			error(&tok.loc, "no type in enum type specifier");
	}
	if (tag)
		t = scopegettag(s, tag, tok.kind != TLBRACE && tok.kind != TSEMICOLON);
	if (t) {
		if (t->kind != kind)
			error(&tok.loc, "redeclaration of tag '%s' with different kind", tag);
	} else {
		if (kind == TYPEENUM) {
			t = mktype(kind, PROPSCALAR|PROPARITH|PROPREAL|PROPINT);
			t->base = et;
		} else {
			t = mktype(kind, 0);
			t->size = 0;
			t->align = 0;
			t->u.structunion.tag = tag;
			t->u.structunion.members = NULL;
		}
		t->incomplete = true;
		if (tag)
			scopeputtag(s, tag, t);
	}
	if (tok.kind != TLBRACE)
		return t;
	if (!t->incomplete)
		error(&tok.loc, "redefinition of tag '%s'", tag);
	next();
	switch (t->kind) {
	case TYPESTRUCT:
	case TYPEUNION:
		b.type = t;
		b.last = &t->u.structunion.members;
		b.bits = 0;
		b.pack = a.kind & ATTRPACKED;
		do structdecl(s, &b);
		while (tok.kind != TRBRACE);
		if (!t->u.structunion.members)
			error(&tok.loc, "struct/union has no members");
		next();
		if (!b.pack)
			t->size = ALIGNUP(t->size, t->align);
		break;
	case TYPEENUM:
		enumconsts = NULL;
		if (et) {
			t->size = t->base->size;
			t->align = t->base->align;
			t->u.basic.issigned = t->base->u.basic.issigned;
			t->incomplete = false;
			et = t;
		} else {
			et = &typeint;
		}
		max = 0;
		min = 0;
		for (value = 0; tok.kind == TIDENT; ++value) {
			name = tok.lit;
			next();
			attr(NULL, 0);
			if (consume(TASSIGN)) {
				e = eval(condexpr(s));
				if (e->kind != EXPRCONST || !(e->type->prop & PROPINT))
					error(&tok.loc, "expected integer constant expression");
				value = e->u.constant.u;
				if (!t->base)
					et = typehasint(&typeint, value, e->type->u.basic.issigned) ? &typeint : e->type;
				else if (!typehasint(et, value, e->type->u.basic.issigned))
					goto invalid;
			} else if (value == 0 && !et->u.basic.issigned || value == 1ull << 63 && et->u.basic.issigned) {
				error(&tok.loc, "no %ssigned integer type can represent enumerator value", et->u.basic.issigned ? "" : "un");
			} else if (!typehasint(et, value, et->u.basic.issigned)) {
				if (t->base) {
				invalid:
					/* fixed underlying type */
					error(&tok.loc, "enumerator '%s' value cannot be represented in underlying type", name);
				}
				sign = et->u.basic.issigned;
				for (i = 0; i < LEN(inttypes); ++i) {
					et = inttypes[i][sign];
					if (typehasint(et, value, sign))
						break;
				}
				assert(i < LEN(inttypes));
			}
			d = mkdecl(name, DECLCONST, et, QUALNONE, LINKNONE);
			d->value = mkintconst(value);
			d->next = enumconsts;
			enumconsts = d;
			if (et->u.basic.issigned && value >= 1ull << 63) {
				if (-value > min)
					min = -value;
			} else if (value > max) {
				max = value;
			}
			scopeputdecl(s, d);
			if (!consume(TCOMMA))
				break;
		}
		expect(TRBRACE, "to close enum specifier");
		if (!t->base) {
			if (min <= 0x80000000 && max <= 0x7fffffff) {
				t->base = min ? &typeint : &typeuint;
			} else {
				sign = min > 0;
				for (i = 0; i < LEN(inttypes); ++i) {
					et = inttypes[i][sign];
					if (typehasint(et, max, false) && typehasint(et, -min, true))
						break;
				}
				if (i == LEN(inttypes))
					error(&tok.loc, "no integer type can represent all enumerator values");
				t->base = et;
				for (d = enumconsts; d; d = d->next)
					d->type = t;
			}
			t->size = t->base->size;
			t->align = t->base->align;
			t->u.basic.issigned = t->base->u.basic.issigned;
		}
	}
	t->incomplete = false;

	return t;
}

/* 6.7 Declarations */
static struct qualtype
declspecs(struct scope *s, enum storageclass *sc, enum funcspec *fs, int *align)
{
	struct type *t, *other;
	struct decl *d;
	struct expr *e;
	enum typespec ts = SPECNONE;
	enum typequal tq = QUALNONE;
	enum tokenkind op;
	int ntypes = 0;
	unsigned long long i;
	struct expr *typeofexpr = NULL;

	t = NULL;
	if (sc)
		*sc = SCNONE;
	if (fs)
		*fs = FUNCNONE;
	if (align)
		*align = 0;
	for (;;) {
		if (typequal(&tq) || storageclass(sc) || funcspec(fs))
			continue;
		op = tok.kind;
		switch (op) {
		/* 6.7.2 Type specifiers */
		case TVOID:
			t = &typevoid;
			++ntypes;
			next();
			break;
		case TCHAR:
			ts |= SPECCHAR;
			++ntypes;
			next();
			break;
		case TSHORT:
			if (ts & SPECSHORT)
				error(&tok.loc, "duplicate 'short'");
			ts |= SPECSHORT;
			next();
			break;
		case TINT:
			ts |= SPECINT;
			++ntypes;
			next();
			break;
		case TLONG:
			if (ts & SPECLONG2)
				error(&tok.loc, "too many 'long'");
			if (ts & SPECLONG)
				ts |= SPECLONG2;
			ts |= SPECLONG;
			next();
			break;
		case TFLOAT:
			ts |= SPECFLOAT;
			++ntypes;
			next();
			break;
		case TDOUBLE:
			ts |= SPECDOUBLE;
			++ntypes;
			next();
			break;
		case TSIGNED:
			if (ts & SPECSIGNED)
				error(&tok.loc, "duplicate 'signed'");
			ts |= SPECSIGNED;
			next();
			break;
		case TUNSIGNED:
			if (ts & SPECUNSIGNED)
				error(&tok.loc, "duplicate 'unsigned'");
			ts |= SPECUNSIGNED;
			next();
			break;
		case TBOOL:
			t = &typebool;
			++ntypes;
			next();
			break;
		case T_COMPLEX:
			error(&tok.loc, "_Complex is not yet supported");
			break;
		case T_ATOMIC:
			error(&tok.loc, "_Atomic is not yet supported");
			break;
		case TSTRUCT:
		case TUNION:
		case TENUM:
			t = tagspec(s);
			++ntypes;
			break;
		case TIDENT:
			if (t || ts)
				goto done;
			d = scopegetdecl(s, tok.lit, 1);
			if (!d || d->kind != DECLTYPE)
				goto done;
			t = d->type;
			tq |= d->qual;
			++ntypes;
			next();
			break;
		case TTYPEOF:
		case TTYPEOF_UNQUAL:
			next();
			expect(TLPAREN, "after 'typeof'");
			t = typename(s, &tq, &typeofexpr);
			if (!t) {
				e = expr(s);
				if (e->decayed)
					e = e->base;
				t = e->type;
				if (op == TTYPEOF)
					tq |= e->qual;
				if (t->prop & PROPVM)
					typeofexpr = e;
			}
			++ntypes;
			expect(TRPAREN, "to close 'typeof'");
			break;

		/* 6.7.5 Alignment specifier */
		case TALIGNAS:
			if (!align)
				error(&tok.loc, "alignment specifier not allowed in this declaration");
			next();
			expect(TLPAREN, "after 'alignas'");
			other = typename(s, NULL, NULL);
			i = other ? other->align : intconstexpr(s, false);
			if (i & i - 1 || i > INT_MAX)
				error(&tok.loc, "invalid alignment: %llu", i);
			if (i > *align)
				*align = i;
			expect(TRPAREN, "to close 'alignas' specifier");
			break;

		case T__ATTRIBUTE__:
			gnuattr(NULL, 0);
			break;

		default:
			goto done;
		}
		if (ntypes > 1 || (t && ts))
			error(&tok.loc, "multiple types in declaration specifiers");
	}
done:
	switch ((int)ts) {
	case SPECNONE:                                            break;
	case SPECCHAR:                          t = &typechar;    break;
	case SPECSIGNED|SPECCHAR:               t = &typeschar;   break;
	case SPECUNSIGNED|SPECCHAR:             t = &typeuchar;   break;
	case SPECSHORT:
	case SPECSHORT|SPECINT:
	case SPECSIGNED|SPECSHORT:
	case SPECSIGNED|SPECSHORT|SPECINT:      t = &typeshort;   break;
	case SPECUNSIGNED|SPECSHORT:
	case SPECUNSIGNED|SPECSHORT|SPECINT:    t = &typeushort;  break;
	case SPECINT:
	case SPECSIGNED:
	case SPECSIGNED|SPECINT:                t = &typeint;     break;
	case SPECUNSIGNED:
	case SPECUNSIGNED|SPECINT:              t = &typeuint;    break;
	case SPECLONG:
	case SPECLONG|SPECINT:
	case SPECSIGNED|SPECLONG:
	case SPECSIGNED|SPECLONG|SPECINT:       t = &typelong;    break;
	case SPECUNSIGNED|SPECLONG:
	case SPECUNSIGNED|SPECLONG|SPECINT:     t = &typeulong;   break;
	case SPECLONGLONG:
	case SPECLONGLONG|SPECINT:
	case SPECSIGNED|SPECLONGLONG:
	case SPECSIGNED|SPECLONGLONG|SPECINT:   t = &typellong;   break;
	case SPECUNSIGNED|SPECLONGLONG:
	case SPECUNSIGNED|SPECLONGLONG|SPECINT: t = &typeullong;  break;
	case SPECFLOAT:                         t = &typefloat;   break;
	case SPECDOUBLE:                        t = &typedouble;  break;
	case SPECLONG|SPECDOUBLE:               t = &typeldouble; break;
	default:
		error(&tok.loc, "invalid combination of type specifiers");
	}
	if (!t && (tq || sc && *sc || fs && *fs))
		error(&tok.loc, "declaration has no type specifier");
	/*
	TODO: consider delaying attribute parsing to declarator(),
	so we can tell the difference between the start of an
	attribute and an array declarator.
	*/
	attr(NULL, 0);

	return (struct qualtype){t, tq, typeofexpr};
}

/* 6.7.6 Declarators */
static struct decl *parameter(struct scope *);

static bool
istypename(struct scope *s, const char *name)
{
	struct decl *d;

	d = scopegetdecl(s, name, 1);
	return d && d->kind == DECLTYPE;
}

/*
When parsing a declarator, qualifiers for derived types are temporarily
stored in the `qual` field of the type itself (elsewhere this field
is used for the qualifiers of the base type). This is corrected in
declarator().
*/
static void
declaratortypes(struct scope *s, struct list *result, char **name, struct scope **funcscope, bool allowabstract)
{
	struct list *ptr, *prev;
	struct type *t;
	struct decl *d, **paramend;
	struct expr *e;
	enum typequal tq;
	bool allowattr;

	while (consume(TMUL)) {
		attr(NULL, 0);
		tq = QUALNONE;
		while (typequal(&tq))
			;
		t = mkpointertype(NULL, tq);
		listinsert(result, &t->link);
	}
	if (name)
		*name = NULL;
	ptr = result->next;
	prev = ptr->prev;
	switch (tok.kind) {
	case TLPAREN:
		next();
		if (allowabstract) {
			switch (tok.kind) {
			case TMUL:
			case TLPAREN:
				break;
			case TIDENT:
				if (!istypename(s, tok.lit))
					break;
				/* fallthrough */
			default:
				allowattr = true;
				goto func;
			}
		}
		declaratortypes(s, result, name, funcscope, allowabstract);
		expect(TRPAREN, "after parenthesized declarator");
		allowattr = false;
		break;
	case TIDENT:
		if (!name)
			error(&tok.loc, "identifier not allowed in abstract declarator");
		*name = tok.lit;
		next();
		allowattr = true;
		break;
	default:
		if (!allowabstract)
			error(&tok.loc, "expected '(' or identifier");
		allowattr = true;
	}
	for (;;) {
		switch (tok.kind) {
		case TLPAREN:  /* function declarator */
			next();
		func:
			t = mktype(TYPEFUNC, 0);
			t->qual = QUALNONE;
			t->u.func.isvararg = false;
			t->u.func.params = NULL;
			t->u.func.nparam = 0;
			paramend = &t->u.func.params;
			s = mkscope(s);
			d = NULL;
			do {
				if (consume(TELLIPSIS)) {
					t->u.func.isvararg = true;
					break;
				}
				if (tok.kind == TRPAREN)
					break;
				d = parameter(s);
				if (d->name)
					scopeputdecl(s, d);
				*paramend = d;
				paramend = &d->next;
				++t->u.func.nparam;
			} while (consume(TCOMMA));
			expect(TRPAREN, "to close function declarator");
			if (funcscope && ptr->prev == prev) {
				/* we may need to re-open the scope later if this is a function definition */
				*funcscope = s;
				s = s->parent;
			} else {
				s = delscope(s);
			}
			if (t->u.func.nparam == 1 && !t->u.func.isvararg && d->type->kind == TYPEVOID && !d->name) {
				t->u.func.params = NULL;
				t->u.func.nparam = 0;
			}
			listinsert(ptr->prev, &t->link);
			allowattr = true;
			break;
		case TLBRACK:  /* array declarator */
			if (allowattr && attr(NULL, 0))
				goto attr;
			next();
			t = mkarraytype(NULL, QUALNONE, 0);
			while (consume(TSTATIC) || typequal(&t->u.array.ptrqual))
				;
			if (tok.kind == TMUL && peek(TRBRACK)) {
				t->prop |= PROPVM;
				t->incomplete = false;
			} else if (!consume(TRBRACK)) {
				e = assignexpr(s);
				if (!(e->type->prop & PROPINT))
					error(&tok.loc, "array length expression must have integer type");
				t->u.array.length = e;
				t->incomplete = false;
				expect(TRBRACK, "after array length");
			}
			listinsert(ptr->prev, &t->link);
			allowattr = true;
			break;
		case T__ATTRIBUTE__:
			if (!allowattr)
				error(&tok.loc, "attribute not allowed after parenthesized declarator");
			/* attribute applies to identifier if ptr->prev == result, otherwise type ptr->prev */
			gnuattr(NULL, 0);
		attr:
			break;
		default:
			return;
		}
	}
}

static struct qualtype
declarator(struct scope *s, struct qualtype base, char **name, struct scope **funcscope, bool allowabstract)
{
	struct type *t;
	enum typequal tq;
	struct expr *e;
	struct list result = {&result, &result}, *l, *prev;

	if (funcscope)
		*funcscope = NULL;
	declaratortypes(s, &result, name, funcscope, allowabstract);
	for (l = result.prev; l != &result; l = prev) {
		prev = l->prev;
		t = listelement(l, struct type, link);
		tq = t->qual;
		t->base = base.type;
		t->qual = base.qual;
		t->prop |= base.type->prop & PROPVM;
		switch (t->kind) {
		case TYPEFUNC:
			if (base.type->kind == TYPEFUNC)
				error(&tok.loc, "function declarator specifies function return type");
			if (base.type->kind == TYPEARRAY)
				error(&tok.loc, "function declarator specifies array return type");
			break;
		case TYPEARRAY:
			if (base.type->incomplete)
				error(&tok.loc, "array element has incomplete type");
			if (base.type->kind == TYPEFUNC)
				error(&tok.loc, "array element has function type");
			t->align = base.type->align;
			t->size = 0;
			if (t->u.array.length) {
				e = eval(t->u.array.length);
				if (e->kind == EXPRCONST) {
					if (e->type->u.basic.issigned && e->u.constant.u >> 63)
						error(&tok.loc, "array length must be non-negative");
					if (e->u.constant.u > ULLONG_MAX / base.type->size)
						error(&tok.loc, "array length is too large");
					t->size = base.type->size * e->u.constant.u;
				} else {
					t->prop |= PROPVM;
					t->u.array.length = e;
					t->u.array.size = NULL;
				}
			}
			break;
		}
		base.type = t;
		base.qual = tq;
	}

	return base;
}

static struct decl *
parameter(struct scope *s)
{
	struct decl *d;
	char *name;
	struct qualtype t;
	enum storageclass sc;

	attr(NULL, 0);
	t = declspecs(s, &sc, NULL, NULL);
	if (!t.type)
		error(&tok.loc, "no type in parameter declaration");
	if (sc && sc != SCREGISTER)
		error(&tok.loc, "parameter declaration has invalid storage-class specifier");
	t = declarator(s, t, &name, NULL, true);
	t.type = typeadjust(t.type, &t.qual);
	d = mkdecl(name, DECLOBJECT, t.type, t.qual, LINKNONE);
	d->u.obj.storage = SDAUTO;
	return d;
}

static void
addmember(struct structbuilder *b, struct qualtype mt, char *name, int align, unsigned long long width)
{
	struct type *t = b->type;
	struct member *m;
	size_t end;

	if (t->flexible)
		error(&tok.loc, "struct has member '%s' after flexible array member", name);
	if (mt.type->incomplete) {
		if (mt.type->kind != TYPEARRAY)
			error(&tok.loc, "struct member '%s' has incomplete type", name);
		t->flexible = true;
	}
	if (mt.type->kind == TYPEFUNC)
		error(&tok.loc, "struct member '%s' has function type", name);
	if (mt.type->prop & PROPVM)
		error(&tok.loc, "struct member '%s' has variably modified type", name);
	if (mt.type->flexible)
		error(&tok.loc, "struct member '%s' contains flexible array member", name);
	assert(mt.type->align > 0);
	if (name || width == -1) {
		m = xmalloc(sizeof(*m));
		m->type = mt.type;
		m->qual = mt.qual;
		m->name = name;
		m->next = NULL;
		*b->last = m;
		b->last = &m->next;
	} else {
		m = NULL;
	}
	if (width == -1) {
		m->bits.before = 0;
		m->bits.after = 0;
		if (align < mt.type->align) {
			if (align)
				error(&tok.loc, "specified alignment of struct member '%s' is less strict than is required by type", name);
			align = b->pack ? 1 : mt.type->align;
		}
		if (t->kind == TYPESTRUCT) {
			m->offset = ALIGNUP(t->size, align);
			t->size = m->offset + mt.type->size;
		} else {
			m->offset = 0;
			if (t->size < mt.type->size)
				t->size = mt.type->size;
		}
		b->bits = 0;
	} else {  /* bit-field */
		if (!(mt.type->prop & PROPINT))
			error(&tok.loc, "bit-field '%s' has invalid type", name);
		if (align)
			error(&tok.loc, "alignment specified for bit-field '%s'", name);
		if (b->pack)
			error(&tok.loc, "bit-field '%s' in packed struct is not supported", name);
		if (!width && name)
			error(&tok.loc, "bit-field '%s' with zero width must not have declarator", name);
		if (width > mt.type->size * 8)
			error(&tok.loc, "bit-field '%s' exceeds width of underlying type", name);
		align = mt.type->align;
		if (t->kind == TYPESTRUCT) {
			/* calculate end of the storage-unit for this bit-field */
			end = ALIGNUP(t->size, mt.type->size);
			if (!width || width > (end - t->size) * 8 + b->bits) {
				/* no room, allocate a new storage-unit */
				t->size = end;
				b->bits = 0;
			}
			if (m) {
				m->offset = ALIGNDOWN(t->size - !!b->bits, mt.type->size);
				m->bits.before = (t->size - m->offset) * 8 - b->bits;
				m->bits.after = mt.type->size * 8 - width - m->bits.before;
			}
			t->size += (width - b->bits + 7) / 8;
			b->bits = (b->bits - width) % 8;
		} else if (m) {
			m->offset = 0;
			m->bits.before = 0;
			m->bits.after = mt.type->size * 8 - width;
			if (t->size < mt.type->size)
				t->size = mt.type->size;
		}
	}
	if (m && t->align < align)
		t->align = align;
}

static bool
staticassert(struct scope *s)
{
	struct stringlit msg;
	unsigned long long c;

	if (!consume(TSTATIC_ASSERT))
		return false;
	expect(TLPAREN, "after static_assert");
	c = intconstexpr(s, true);
	if (consume(TCOMMA)) {
		tokencheck(&tok, TSTRINGLIT, "after static assertion expression");
		stringconcat(&msg, true);
		if (!c)
			error(&tok.loc, "static assertion failed: %.*s", (int)(msg.size - 1), (char *)msg.data);
	} else if (!c) {
		error(&tok.loc, "static assertion failed");
	}
	expect(TRPAREN, "after static assertion");
	expect(TSEMICOLON, "after static assertion");
	return true;
}

static void
structdecl(struct scope *s, struct structbuilder *b)
{
	struct qualtype base, mt;
	char *name;
	unsigned long long width;
	int align;

	if (staticassert(s))
		return;
	attr(NULL, 0);
	base = declspecs(s, NULL, NULL, &align);
	if (!base.type)
		error(&tok.loc, "no type in struct member declaration");
	if (tok.kind == TSEMICOLON) {
		if ((base.type->kind != TYPESTRUCT && base.type->kind != TYPEUNION) || base.type->u.structunion.tag)
			error(&tok.loc, "struct declaration must declare at least one member");
		next();
		addmember(b, base, NULL, align, -1);
		return;
	}
	for (;;) {
		if (consume(TCOLON)) {
			width = intconstexpr(s, false);
			addmember(b, base, NULL, 0, width);
		} else {
			mt = declarator(s, base, &name, NULL, false);
			width = consume(TCOLON) ? intconstexpr(s, false) : -1;
			addmember(b, mt, name, align, width);
		}
		if (tok.kind == TSEMICOLON)
			break;
		expect(TCOMMA, "or ';' after declarator");
	}
	next();
}

/* 6.7.7 Type names */
struct type *
typename(struct scope *s, enum typequal *tq, struct expr **toeval)
{
	struct qualtype t;

	t = declspecs(s, NULL, NULL, NULL);
	if (t.type) {
		t = declarator(s, t, NULL, NULL, true);
		if (tq)
			*tq |= t.qual;
		if (toeval)
			*toeval = t.expr;
	}
	return t.type;
}

static enum linkage
getlinkage(enum declkind kind, enum storageclass sc, struct decl *prior, bool filescope)
{
	if (sc & SCSTATIC)
		return filescope ? LINKINTERN : LINKNONE;
	if (sc & SCEXTERN || kind == DECLFUNC)
		return prior ? prior->linkage : LINKEXTERN;
	return filescope ? LINKEXTERN : LINKNONE;
}

static struct decl *
declcommon(struct scope *s, enum declkind kind, char *name, char *asmname, struct type *t, enum typequal tq, enum storageclass sc, struct decl *prior)
{
	struct decl *d;
	enum linkage linkage;
	const char *kindstr = kind == DECLFUNC ? "function" : "object";

	if (prior) {
		if (prior->linkage == LINKNONE)
			error(&tok.loc, "%s '%s' with no linkage redeclared", kindstr, name);
		linkage = getlinkage(kind, sc, prior, s == &filescope);
		if (prior->linkage != linkage)
			error(&tok.loc, "%s '%s' redeclared with different linkage", kindstr, name);
		if (!typecompatible(t, prior->type) || tq != prior->qual)
			error(&tok.loc, "%s '%s' redeclared with incompatible type", kindstr, name);
		if (asmname && (!prior->asmname || strcmp(prior->asmname, asmname) != 0))
			error(&tok.loc, "%s '%s' redeclared with different assembler name", kindstr, name);
		prior->type = typecomposite(t, prior->type);
		return prior;
	}
	if (s->parent)
		prior = scopegetdecl(s->parent, name, true);
	linkage = getlinkage(kind, sc, prior, s == &filescope);
	if (linkage != LINKNONE && s->parent) {
		/* XXX: should maintain map of identifiers with linkage to their declaration, and use that */
		if (s->parent != &filescope)
			prior = scopegetdecl(&filescope, name, false);
		if (prior && prior->linkage != LINKNONE) {
			if (prior->kind != kind)
				error(&tok.loc, "'%s' redeclared with different kind", name);
			if (prior->linkage != linkage)
				error(&tok.loc, "%s '%s' redeclared with different linkage", kindstr, name);
			if (!typecompatible(t, prior->type) || tq != prior->qual)
				error(&tok.loc, "%s '%s' redeclared with incompatible type", kindstr, name);
			if (!asmname)
				asmname = prior->asmname;
			else if (!prior->asmname || strcmp(prior->asmname, asmname) != 0)
				error(&tok.loc, "%s '%s' redeclared with different assembler name", kindstr, name);
			t = typecomposite(t, prior->type);
		}
	}
	d = mkdecl(name, kind, t, tq, linkage);
	d->asmname = asmname;
	scopeputdecl(s, d);
	return d;
}

bool
decl(struct scope *s, struct func *f)
{
	struct qualtype base, qt;
	struct type *t;
	enum typequal tq;
	enum storageclass sc;
	enum funcspec fs;
	struct init *init;
	bool hasinit;
	char *name, *asmname;
	int allowfunc = !f;
	struct decl *d, *prior;
	enum declkind kind;
	struct scope *funcscope;
	int align;

	if (staticassert(s))
		return true;
	if (attr(NULL, 0) && consume(TSEMICOLON))
		return true;
	base = declspecs(s, &sc, &fs, &align);
	if (!base.type)
		return false;
	if (f) {
		if (sc == SCTHREADLOCAL)
			error(&tok.loc, "block scope declaration containing 'thread_local' must contain 'static' or 'extern'");
	} else {
		/* 6.9p2 */
		if (sc & SCAUTO)
			error(&tok.loc, "external declaration must not contain 'auto'");
		if (sc & SCREGISTER)
			error(&tok.loc, "external declaration must not contain 'register'");
	}
	if (consume(TSEMICOLON)) {
		/* XXX 6.7p2 error unless in function parameter/struct/union, or tag/enum members are declared */
		return true;
	}
	for (;;) {
		qt = declarator(s, base, &name, &funcscope, false);
		t = qt.type;
		tq = qt.qual;
		if (consume(T__ASM__)) {
			expect(TLPAREN, "after __asm__");
			asmname = expect(TSTRINGLIT, "for assembler name");
			expect(TRPAREN, "after assembler name");
			allowfunc = 0;
		} else {
			asmname = NULL;
		}
		kind = sc & SCTYPEDEF ? DECLTYPE : t->kind == TYPEFUNC ? DECLFUNC : DECLOBJECT;
		prior = scopegetdecl(s, name, false);
		if (prior && prior->kind != kind)
			error(&tok.loc, "'%s' redeclared with different kind", name);
		switch (kind) {
		case DECLTYPE:
			if (align)
				error(&tok.loc, "typedef '%s' declared with alignment specifier", name);
			if (asmname)
				error(&tok.loc, "typedef '%s' declared with assembler label", name);
			if (!prior)
				scopeputdecl(s, mkdecl(name, DECLTYPE, t, tq, LINKNONE));
			else if (!typesame(prior->type, t) || prior->qual != tq)
				error(&tok.loc, "typedef '%s' redefined with different type", name);
			break;
		case DECLOBJECT:
			if (align && align < t->align)
				error(&tok.loc, "object '%s' requires alignment %d, which is stricter than specified alignment %d", name, t->align, align);
			d = declcommon(s, kind, name, asmname, t, tq, sc, prior);
			if (d->u.obj.align < align)
				d->u.obj.align = align;
			if (d->linkage == LINKNONE && !(sc & SCSTATIC)) {
				d->u.obj.storage = SDAUTO;
			} else {
				d->u.obj.storage = sc & SCTHREADLOCAL ? SDTHREAD : SDSTATIC;
				if (t->prop & PROPVM)
					error(&tok.loc, "object '%s' with %s storage duration cannot have variably modified type", name, d->u.obj.storage == SDSTATIC ? "static" : "thread");
				d->value = mkglobal(d);
			}

			if (base.expr)
				funcexpr(f, base.expr);
			init = NULL;
			hasinit = false;
			if (consume(TASSIGN)) {
				if (f && d->linkage != LINKNONE)
					error(&tok.loc, "object '%s' with block scope and %s linkage cannot have initializer", name, d->linkage == LINKEXTERN ? "external" : "internal");
				if (d->defined)
					error(&tok.loc, "object '%s' redefined", name);
				init = parseinit(s, d->type);
				hasinit = true;
			} else if (sc & SCEXTERN) {
				break;
			} else if (d->linkage != LINKNONE && d->u.obj.storage == SDSTATIC) {
				if (!d->defined && !d->tentative) {
					d->tentative = true;
					*tentativedefnsend = d;
					tentativedefnsend = &d->next;
				}
				break;
			}
			if (d->u.obj.storage == SDAUTO)
				funcinit(f, d, init, hasinit);
			else
				emitdata(d, init);
			d->defined = true;
			break;
		case DECLFUNC:
			if (align)
				error(&tok.loc, "function '%s' declared with alignment specifier", name);
			if (f && sc && sc != SCEXTERN)  /* 6.7.1p7 */
				error(&tok.loc, "function '%s' with block scope may only have storage class 'extern'", name);
			d = declcommon(s, kind, name, asmname, t, tq, sc, prior);
			d->value = mkglobal(d);
			d->u.func.inlinedefn = d->linkage == LINKEXTERN && fs & FUNCINLINE && !(sc & SCEXTERN) && (!prior || prior->u.func.inlinedefn);
			d->u.func.isnoreturn = fs & FUNCNORETURN;
			if (tok.kind == TLBRACE) {
				if (!allowfunc)
					error(&tok.loc, "function definition not allowed");
				if (d->defined)
					error(&tok.loc, "function '%s' redefined", name);
				/* re-open scope from function declarator */
				assert(funcscope);
				s = funcscope;
				f = mkfunc(d, name, t, s);
				stmt(f, s);
				if (d->u.func.isnoreturn)
					funchlt(f);
				/* XXX: need to keep track of function in case a later declaration specifies extern */
				if (!d->u.func.inlinedefn)
					emitfunc(f, d->linkage == LINKEXTERN);
				s = delscope(s);
				delfunc(f);
				d->defined = true;
				return true;
			} else if (funcscope) {
				delscope(funcscope);
			}
			break;
		}
		if (consume(TSEMICOLON))
			return true;
		expect(TCOMMA, "or ';' after declarator");
		allowfunc = 0;
	}
}

struct decl *
stringdecl(struct expr *expr)
{
	static struct map strings;
	struct mapkey key;
	void **entry;
	struct decl *d;

	if (!strings.len)
		mapinit(&strings, 64);
	assert(expr->kind == EXPRSTRING);
	mapkey(&key, expr->u.string.data, expr->u.string.size);
	entry = mapput(&strings, &key);
	d = *entry;
	if (!d) {
		d = mkdecl("string", DECLOBJECT, expr->type, QUALNONE, LINKNONE);
		d->value = mkglobal(d);
		emitdata(d, mkinit(0, expr->type->size, (struct bitfield){0}, expr));
		*entry = d;
	}
	return d;
}

void
emittentativedefns(void)
{
	struct decl *d;

	for (d = tentativedefns; d; d = d->next) {
		if (!d->defined)
			emitdata(d, NULL);
	}
}
