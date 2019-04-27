#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "cc.h"

static struct list tentativedefns = {&tentativedefns, &tentativedefns};

struct qualtype {
	struct type *type;
	enum typequal qual;
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
};

struct decl *
mkdecl(enum declkind k, struct type *t, enum typequal tq, enum linkage linkage)
{
	struct decl *d;

	d = xmalloc(sizeof(*d));
	d->kind = k;
	d->linkage = linkage;
	d->type = t;
	d->qual = tq;
	d->tentative = false;
	d->defined = false;
	d->align = 0;
	d->value = NULL;

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
	case T_THREAD_LOCAL: new = SCTHREADLOCAL; break;
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

static struct type *
tagspec(struct scope *s)
{
	struct type *t;
	char *tag;
	enum typekind kind;
	struct decl *d;
	struct expr *e;
	struct structbuilder b;
	uint64_t i;

	switch (tok.kind) {
	case TSTRUCT: kind = TYPESTRUCT; break;
	case TUNION:  kind = TYPEUNION;  break;
	case TENUM:   kind = TYPEENUM;   break;
	default: fatal("internal error: unknown tag kind");
	}
	next();
	if (tok.kind == TIDENT) {
		tag = tok.lit;
		next();
		t = scopegettag(s, tag, false);
		if (s->parent && !t && tok.kind != TLBRACE && (kind == TYPEENUM || tok.kind != TSEMICOLON))
			t = scopegettag(s->parent, tag, true);
	} else if (tok.kind != TLBRACE) {
		error(&tok.loc, "expected identifier or '{' after struct/union");
	} else {
		tag = NULL;
		t = NULL;
	}
	if (t) {
		if (t->kind != kind)
			error(&tok.loc, "redeclaration of tag '%s' with different kind", tag);
	} else {
		if (kind == TYPEENUM) {
			t = xmalloc(sizeof(*t));
			*t = typeuint;
			t->kind = kind;
		} else {
			t = mktype(kind, PROPOBJECT);
			if (kind == TYPESTRUCT)
				t->prop |= PROPAGGR;
			t->repr = &i64; // XXX
			t->size = 0;
			t->align = 0;
			t->structunion.tag = tag;
			t->structunion.members = NULL;
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
		b.last = &t->structunion.members;
		b.bits = 0;
		do structdecl(s, &b);
		while (tok.kind != TRBRACE);
		next();
		t->size = ALIGNUP(t->size, t->align);
		t->incomplete = false;
		break;
	case TYPEENUM:
		for (i = 0; tok.kind == TIDENT; ++i) {
			d = mkdecl(DECLCONST, &typeint, QUALNONE, LINKNONE);
			scopeputdecl(s, tok.lit, d);
			next();
			if (consume(TASSIGN)) {
				e = constexpr(s);
				if (e->kind != EXPRCONST || !(e->type->prop & PROPINT))
					error(&tok.loc, "expected integer constant expression");
				if (e->type->basic.issigned && e->constant.i >= 1ull << 63)
					t->basic.issigned = true;
				i = e->constant.i;
			}
			d->value = mkintconst(t->repr, i);
			if (!consume(TCOMMA))
				break;
		}
		expect(TRBRACE, "to close enum specifier");
		t->incomplete = false;
	}

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
	int ntypes = 0;
	uint64_t i;

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
		switch (tok.kind) {
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
		case T_BOOL:
			t = &typebool;
			++ntypes;
			next();
			break;
		case T_COMPLEX:
			fatal("_Complex is not yet supported");
			break;
		case T_ATOMIC:
			fatal("_Atomic is not yet supported");
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
			++ntypes;
			next();
			break;
		case T__TYPEOF__:
			next();
			expect(TLPAREN, "after '__typeof__'");
			t = typename(s, &tq);
			if (!t) {
				e = expr(s);
				if (e->decayed)
					e = e->base;
				t = e->type;
				tq |= e->qual;
				delexpr(e);
			}
			++ntypes;
			expect(TRPAREN, "to close '__typeof__'");
			break;

		/* 6.7.5 Alignment specifier */
		case T_ALIGNAS:
			if (!align)
				error(&tok.loc, "alignment specifier not allowed in this declaration");
			next();
			expect(TLPAREN, "after '_Alignas'");
			other = typename(s, NULL);
			if (other) {
				*align = other->align;
			} else {
				i = intconstexpr(s, false);
				if (!i || i & (i - 1) || i > 16)
					error(&tok.loc, "invalid alignment: %d", i);
				*align = (int)i;
			}
			expect(TRPAREN, "to close '_Alignas' specifier");
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
	if (!t && (tq || (sc && *sc) || (fs && *fs)))
		error(&tok.loc, "declaration has no type specifier");

	return (struct qualtype){t, tq};
}

/* 6.7.6 Declarators */
static struct param *parameter(struct scope *);

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
declaratortypes(struct scope *s, struct list *result, char **name, bool allowabstract)
{
	struct list *ptr;
	struct type *t;
	struct param **p;
	uint64_t i;
	enum typequal tq;

	while (consume(TMUL)) {
		tq = QUALNONE;
		while (typequal(&tq))
			;
		t = mkpointertype(NULL, tq);
		listinsert(result, &t->link);
	}
	if (name)
		*name = NULL;
	ptr = result->next;
	switch (tok.kind) {
	case TLPAREN:
		next();
		switch (tok.kind) {
		case TMUL:
		case TLPAREN:
			break;
		case TIDENT:
			if (!allowabstract || !istypename(s, tok.lit))
				break;
			/* fallthrough */
		default:
			goto func;
		}
		declaratortypes(s, result, name, allowabstract);
		expect(TRPAREN, "after parenthesized declarator");
		break;
	case TIDENT:
		if (!name)
			error(&tok.loc, "identifier not allowed in abstract declarator");
		*name = tok.lit;
		next();
		break;
	default:
		if (!allowabstract)
			error(&tok.loc, "expected '(' or identifier");
	}
	for (;;) {
		switch (tok.kind) {
		case TLPAREN:  /* function declarator */
			next();
		func:
			t = mktype(TYPEFUNC, PROPDERIVED);
			t->qual = QUALNONE;
			t->func.isprototype = false;
			t->func.isvararg = false;
			t->func.isnoreturn = false;
			t->func.params = NULL;
			p = &t->func.params;
			switch (tok.kind) {
			case TIDENT:
				if (!istypename(s, tok.lit)) {
					/* identifier-list (K&R declaration) */
					do {
						*p = mkparam(tok.lit, NULL, QUALNONE);
						p = &(*p)->next;
						next();
					} while (consume(TCOMMA) && tok.kind == TIDENT);
					break;
				}
				/* fallthrough */
			default:
				t->func.isprototype = true;
				for (;;) {
					*p = parameter(s);
					p = &(*p)->next;
					if (!consume(TCOMMA))
						break;
					if (consume(TELLIPSIS)) {
						t->func.isvararg = true;
						break;
					}
				}
				if (t->func.params->type->kind == TYPEVOID && !t->func.params->next)
					t->func.params = NULL;
				break;
			case TRPAREN:
				break;
			}
			expect(TRPAREN, "to close function declarator");
			t->func.paraminfo = t->func.isprototype || t->func.params || tok.kind == TLBRACE;
			listinsert(ptr->prev, &t->link);
			break;
		case TLBRACK:  /* array declarator */
			next();
			tq = QUALNONE;
			while (consume(TSTATIC) || typequal(&tq))
				;
			if (tok.kind == TMUL)
				error(&tok.loc, "VLAs are not yet supported");
			if (tok.kind == TRBRACK) {
				i = 0;
				next();
			} else {
				i = intconstexpr(s, false);
				expect(TRBRACK, "after array length");
			}
			t = mkarraytype(NULL, tq, i);
			listinsert(ptr->prev, &t->link);
			break;
		default:
			return;
		}
	}
}

static struct qualtype
declarator(struct scope *s, struct qualtype base, char **name, bool allowabstract)
{
	struct type *t;
	enum typequal tq;
	struct list result = {&result, &result}, *l, *prev;

	declaratortypes(s, &result, name, allowabstract);
	for (l = result.prev; l != &result; l = prev) {
		prev = l->prev;
		t = listelement(l, struct type, link);
		tq = t->qual;
		t->base = base.type;
		t->qual = base.qual;
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
			t->size = base.type->size * t->array.length;  // XXX: overflow?
			break;
		}
		base.type = t;
		base.qual = tq;
	}

	return base;
}

static struct type *
adjust(struct type *t)
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

static struct param *
parameter(struct scope *s)
{
	char *name;
	struct qualtype t;
	enum storageclass sc;

	t = declspecs(s, &sc, NULL, NULL);
	if (!t.type)
		error(&tok.loc, "no type in parameter declaration");
	if (sc && sc != SCREGISTER)
		error(&tok.loc, "parameter declaration has invalid storage-class specifier");
	t = declarator(s, t, &name, true);

	return mkparam(name, adjust(t.type), t.qual);
}

static bool
paramdecl(struct scope *s, struct param *params)
{
	struct param *p;
	struct qualtype t, base;
	enum storageclass sc;
	char *name;

	base = declspecs(s, &sc, NULL, NULL);
	if (!base.type)
		return false;
	if (sc && sc != SCREGISTER)
		error(&tok.loc, "parameter declaration has invalid storage-class specifier");
	for (;;) {
		t = declarator(s, base, &name, false);
		for (p = params; p && strcmp(name, p->name) != 0; p = p->next)
			;
		if (!p)
			error(&tok.loc, "old-style function declarator has no parameter named '%s'", name);
		p->type = adjust(t.type);
		p->qual = t.qual;
		if (tok.kind == TSEMICOLON)
			break;
		expect(TCOMMA, "or ';' after parameter declarator");
	}
	next();
	return true;
}

static void
addmember(struct structbuilder *b, struct qualtype mt, char *name, int align, uint64_t width)
{
	struct type *t = b->type;
	struct member *m;
	size_t end;

	// XXX: check incomplete type, except for flexible array member
	if (mt.type->kind == TYPEFUNC)
		error(&tok.loc, "struct member '%s' has function type", name);
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
		if (align < mt.type->align)
			align = mt.type->align;
		t->size = ALIGNUP(t->size, align);
		if (t->kind == TYPESTRUCT) {
			m->offset = t->size;
			t->size += mt.type->size;
		} else {
			m->offset = 0;
			if (t->size < mt.type->size)
				t->size = mt.type->size;
		}
	} else {  /* bit-field */
		if (!(mt.type->prop & PROPINT))
			error(&tok.loc, "bit-field has invalid type");
		if (align)
			error(&tok.loc, "alignment specified for bit-field");
		if (!width && name)
			error(&tok.loc, "bit-field with zero width must not have declarator");
		if (width > mt.type->size * 8)
			error(&tok.loc, "bit-field exceeds width of underlying type");
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
		} else {
			if (m) {
				m->offset = 0;
				m->bits.before = 0;
				m->bits.after = mt.type->size * 8 - width;
			}
			if (t->size < mt.type->size)
				t->size = mt.type->size;
		}
	}
	if (t->align < align)
		t->align = align;
}

static void
structdecl(struct scope *s, struct structbuilder *b)
{
	struct qualtype base, mt;
	char *name;
	uint64_t width;
	int align;

	base = declspecs(s, NULL, NULL, &align);
	if (!base.type)
		error(&tok.loc, "no type in struct member declaration");
	if (tok.kind == TSEMICOLON) {
		if ((base.type->kind != TYPESTRUCT && base.type->kind != TYPEUNION) || base.type->structunion.tag)
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
			mt = declarator(s, base, &name, false);
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
typename(struct scope *s, enum typequal *tq)
{
	struct qualtype t;

	t = declspecs(s, NULL, NULL, NULL);
	if (t.type) {
		t = declarator(s, t, NULL, true);
		if (tq)
			*tq |= t.qual;
	}
	return t.type;
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
	struct param *p;
	char *name;
	int allowfunc = !f;
	struct decl *d;
	enum declkind kind;
	enum linkage linkage;
	uint64_t c;
	int align;

	if (consume(T_STATIC_ASSERT)) {
		expect(TLPAREN, "after _Static_assert");
		c = intconstexpr(s, true);
		expect(TCOMMA, "after static assertion expression");
		expect(TSTRINGLIT, "after static assertion expression");
		if (!c)
			error(&tok.loc, "static assertion failed");  // XXX: add string here
		expect(TRPAREN, "after static assertion message");
		expect(TSEMICOLON, "after static assertion");
		return true;
	}
	base = declspecs(s, &sc, &fs, &align);
	if (!base.type)
		return false;
	if (!f) {
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
		qt = declarator(s, base, &name, false);
		t = qt.type;
		tq = qt.qual;
		kind = sc & SCTYPEDEF ? DECLTYPE : t->kind == TYPEFUNC ? DECLFUNC : DECLOBJECT;
		d = scopegetdecl(s, name, false);
		if (d && d->kind != kind)
			error(&tok.loc, "'%s' redeclared with different kind", name);
		switch (kind) {
		case DECLTYPE:
			if (align)
				error(&tok.loc, "typedef '%s' declared with alignment specifier", name);
			if (!d)
				scopeputdecl(s, name, mkdecl(DECLTYPE, t, tq, LINKNONE));
			else if (!typesame(d->type, t) || d->qual != tq)
				error(&tok.loc, "typedef '%s' redefined with different type", name);
			break;
		case DECLOBJECT:
			if (d) {
				if (d->linkage == LINKNONE)
					error(&tok.loc, "object '%s' with no linkage redeclared", name);
				if (!(sc & SCEXTERN)) {
					linkage = f ? LINKNONE : sc & SCSTATIC ? LINKINTERN : LINKEXTERN;
					if (d->linkage != linkage)
						error(&tok.loc, "object '%s' redeclared with different linkage", name);
				}
				if (!typecompatible(d->type, t) || d->qual != tq)
					error(&tok.loc, "object '%s' redeclared with incompatible type", name);
				d->type = typecomposite(t, d->type);
			} else {
				if (sc & SCEXTERN) {
					if (s->parent)
						d = scopegetdecl(s->parent, name, true);
					linkage = d && d->linkage != LINKNONE ? d->linkage : LINKEXTERN;
					d = scopegetdecl(&filescope, name, false);
					if (d) {
						if (d->linkage != linkage)
							error(&tok.loc, "object '%s' redeclared with different linkage", name);
						if (!typecompatible(d->type, t) || d->qual != tq)
							error(&tok.loc, "object '%s' redeclared with incompatible type", name);
						t = typecomposite(t, d->type);
					}
				} else {
					linkage = f ? LINKNONE : sc & SCSTATIC ? LINKINTERN : LINKEXTERN;
				}

				d = mkdecl(kind, t, tq, linkage);
				scopeputdecl(s, name, d);
				if (linkage != LINKNONE || sc & SCSTATIC)
					d->value = mkglobal(name, linkage == LINKNONE);
			}
			if (d->align < align)
				d->align = align;
			if (consume(TASSIGN)) {
				if (f && d->linkage != LINKNONE)
					error(&tok.loc, "object '%s' with block scope and %s linkage cannot have initializer", name, d->linkage == LINKEXTERN ? "external" : "internal");
				if (d->defined)
					error(&tok.loc, "object '%s' redefined", name);
				init = parseinit(s, d->type);
			} else {
				init = NULL;
			}
			if (sc & SCEXTERN)
				break;
			if (init || f) {
				if (d->linkage != LINKNONE || sc & SCSTATIC)
					emitdata(d, init);
				else
					funcinit(f, d, init);
				d->defined = true;
				if (d->tentative) {
					d->tentative = false;
					listremove(&d->link);
				}
			} else if (!d->defined && !d->tentative) {
				d->tentative = true;
				listinsert(tentativedefns.prev, &d->link);
			}
			break;
		case DECLFUNC:
			if (align)
				error(&tok.loc, "function '%s' declared with alignment specifier", name);
			t->func.isnoreturn |= fs & FUNCNORETURN;
			if (f && sc && sc != SCEXTERN)  /* 6.7.1p7 */
				error(&tok.loc, "function '%s' with block scope may only have storage class 'extern'", name);
			if (!t->func.isprototype && t->func.params) {
				if (!allowfunc)
					error(&tok.loc, "function declaration not allowed");
				/* collect type information for parameters before we check compatibility */
				while (paramdecl(s, t->func.params))
					;
				if (tok.kind != TLBRACE)
					error(&tok.loc, "function declaration with identifier list is not part of definition");
				for (p = t->func.params; p; p = p->next) {
					if (!p->type)
						error(&tok.loc, "old-style function definition does not declare '%s'", p->name);
				}
			}
			if (d) {
				if (!typecompatible(t, d->type) || tq != d->qual)
					error(&tok.loc, "function '%s' redeclared with incompatible type", name);
				d->type = typecomposite(t, d->type);
			} else {
				if (s->parent)
					d = scopegetdecl(s->parent, name, 1);
				if (d && d->linkage != LINKNONE) {
					linkage = d->linkage;
					if (!typecompatible(t, d->type) || tq != d->qual)
						error(&tok.loc, "function '%s' redeclared with incompatible type", name);
					t = typecomposite(t, d->type);
				} else {
					linkage = sc & SCSTATIC ? LINKINTERN : LINKEXTERN;
				}
				d = mkdecl(kind, t, tq, linkage);
				d->value = mkglobal(name, false);
				scopeputdecl(s, name, d);
			}
			if (tok.kind == TLBRACE) {
				if (!allowfunc)
					error(&tok.loc, "function declaration not allowed");
				if (d->defined)
					error(&tok.loc, "function '%s' redefined", name);
				s = mkscope(&filescope);
				f = mkfunc(name, t, s);
				stmt(f, s);
				emitfunc(f, d->linkage == LINKEXTERN);
				s = delscope(s);
				delfunc(f);
				d->defined = true;
				return true;
			}
			break;
		}
		switch (tok.kind) {
		case TCOMMA:
			next();
			allowfunc = 0;
			break;
		case TSEMICOLON:
			next();
			return true;
		default:
			error(&tok.loc, "expected ',' or ';' after declarator");
		}
	}
}

struct decl *stringdecl(struct expr *expr)
{
	static struct map *strings;
	struct mapkey key;
	void **entry;
	struct decl *d;

	if (!strings)
		strings = mkmap(64);
	assert(expr->kind == EXPRSTRING);
	mapkey(&key, expr->string.data, expr->string.size);
	entry = mapput(strings, &key);
	d = *entry;
	if (!d) {
		d = mkdecl(DECLOBJECT, expr->type, QUALNONE, LINKNONE);
		d->value = mkglobal("string", true);
		emitdata(d, mkinit(0, expr->type->size, (struct bitfield){0}, expr));
		*entry = d;
	}
	return d;
}

void
emittentativedefns(void)
{
	struct list *l;

	for (l = tentativedefns.next; l != &tentativedefns; l = l->next)
		emitdata(listelement(l, struct decl, link), NULL);
}
