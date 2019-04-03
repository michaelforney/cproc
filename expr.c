#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "util.h"
#include "decl.h"
#include "eval.h"
#include "expr.h"
#include "init.h"
#include "pp.h"
#include "scope.h"
#include "token.h"
#include "type.h"

static struct expr *
mkexpr(enum exprkind k, struct type *t, enum exprflags flags)
{
	struct expr *e;

	e = xmalloc(sizeof(*e));
	e->type = flags & EXPRFLAG_LVAL || !t ? t : typeunqual(t, NULL);
	e->flags = flags;
	e->kind = k;
	e->next = NULL;

	return e;
}

static struct expr *
mkconstexpr(struct type *t, uint64_t n)
{
	struct expr *e;

	e = mkexpr(EXPRCONST, t, 0);
	e->constant.i = n;

	return e;
}

void
delexpr(struct expr *e)
{
	free(e);
}

static struct expr *mkunaryexpr(enum tokenkind, struct expr *);

/* 6.3.2.1 Conversion of arrays and function designators */
static struct expr *
decay(struct expr *e)
{
	struct type *t;
	enum typequal tq = QUALNONE;

	// XXX: combine with decl.c:adjust in some way?
	t = typeunqual(e->type, &tq);
	switch (t->kind) {
	case TYPEARRAY:
		e = mkunaryexpr(TBAND, e);
		e->type = mkqualifiedtype(mkpointertype(t->base), tq);
		e->flags |= EXPRFLAG_DECAYED;
		break;
	case TYPEFUNC:
		e = mkunaryexpr(TBAND, e);
		e->flags |= EXPRFLAG_DECAYED;
		break;
	}

	return e;
}

static void
lvalueconvert(struct expr *e)
{
	e->type = typeunqual(e->type, NULL);
	e->flags &= ~EXPRFLAG_LVAL;
}

static struct expr *
mkunaryexpr(enum tokenkind op, struct expr *base)
{
	struct expr *expr;

	switch (op) {
	case TBAND:
		if (base->flags & EXPRFLAG_DECAYED) {
			base->flags &= ~EXPRFLAG_DECAYED;
			return base;
		}
		expr = mkexpr(EXPRUNARY, mkpointertype(base->type), 0);
		expr->unary.op = op;
		expr->unary.base = base;
		return expr;
	case TMUL:
		lvalueconvert(base);
		if (base->type->kind != TYPEPOINTER)
			error(&tok.loc, "cannot dereference non-pointer");
		expr = mkexpr(EXPRUNARY, base->type->base, EXPRFLAG_LVAL);
		expr->unary.op = op;
		expr->unary.base = base;
		return decay(expr);
	}
	/* other unary operators get compiled as equivalent binary ones */
	fatal("internal error: unknown unary operator %d", op);
}

static struct type *
commonreal(struct expr **e1, struct expr **e2)
{
	struct type *t;

	t = typecommonreal((*e1)->type, (*e2)->type);
	*e1 = exprconvert(*e1, t);
	*e2 = exprconvert(*e2, t);

	return t;
}

static struct expr *
mkbinaryexpr(struct location *loc, enum tokenkind op, struct expr *l, struct expr *r)
{
	struct expr *e;
	struct type *t = NULL;
	enum typeprop lp, rp;

	lvalueconvert(l);
	lvalueconvert(r);
	lp = typeprop(l->type);
	rp = typeprop(r->type);
	switch (op) {
	case TLOR:
	case TLAND:
		if (!(lp & PROPSCALAR))
			error(loc, "left-hand-side of logical operator must be scalar");
		if (!(rp & PROPSCALAR))
			error(loc, "right-hand-side of logical operator must be scalar");
		l = exprconvert(l, &typebool);
		r = exprconvert(r, &typebool);
		t = &typeint;
		break;
	case TEQL:
	case TNEQ:
	case TLESS:
	case TGREATER:
	case TLEQ:
	case TGEQ:
		if (lp & PROPARITH && rp & PROPARITH) {
			commonreal(&l, &r);
		} else {
			// XXX: check pointer types
		}
		t = &typeint;
		break;
	case TBOR:
	case TXOR:
	case TBAND:
		t = commonreal(&l, &r);
		break;
	case TADD:
		if (lp & PROPARITH && rp & PROPARITH) {
			t = commonreal(&l, &r);
			break;
		}
		if (r->type->kind == TYPEPOINTER)
			e = l, l = r, r = e, rp = lp;
		if (l->type->kind != TYPEPOINTER || !(rp & PROPINT))
			error(loc, "invalid operands to '+' operator");
		t = l->type;
		if (t->base->incomplete)
			error(loc, "pointer operand to '+' must be to complete object type");
		r = mkbinaryexpr(loc, TMUL, exprconvert(r, &typeulong), mkconstexpr(&typeulong, t->base->size));
		break;
	case TSUB:
		if (lp & PROPARITH && rp & PROPARITH) {
			t = commonreal(&l, &r);
			break;
		}
		if (l->type->kind != TYPEPOINTER || !(rp & PROPINT) && r->type->kind != TYPEPOINTER)
			error(loc, "invalid operands to '-' operator");
		if (l->type->base->incomplete)
			error(loc, "pointer operand to '-' must be to complete object type");
		if (rp & PROPINT) {
			t = l->type;
			r = mkbinaryexpr(loc, TMUL, exprconvert(r, &typeulong), mkconstexpr(&typeulong, t->base->size));
		} else {
			if (!typecompatible(typeunqual(l->type->base, NULL), typeunqual(r->type->base, NULL)))
				error(&tok.loc, "pointer operands to '-' are to incompatible types");
			op = TDIV;
			t = &typelong;
			e = mkbinaryexpr(loc, TSUB, exprconvert(l, &typelong), exprconvert(r, &typelong));
			r = mkconstexpr(&typelong, l->type->base->size);
			l = e;
		}
		break;
	case TMOD:
		if (!(lp & PROPINT) || !(rp & PROPINT))
			error(loc, "operands to '%%' operator must be integer");
		t = commonreal(&l, &r);
		break;
	case TMUL:
	case TDIV:
		if (!(lp & PROPARITH) || !(rp & PROPARITH))
			error(loc, "operands to '%c' operator must be arithmetic", op == TMUL ? '*' : '/');
		t = commonreal(&l, &r);
		break;
	case TSHL:
	case TSHR:
		if (!(lp & PROPINT) || !(rp & PROPINT))
			error(loc, "operands to '%s' operator must be integer", op == TSHL ? "<<" : ">>");
		l = exprconvert(l, typeintpromote(l->type));
		r = exprconvert(r, typeintpromote(r->type));
		t = l->type;
		break;
	default:
		fatal("internal error: unknown binary operator %d", op);
	}
	e = mkexpr(EXPRBINARY, t, 0);
	e->binary.op = op;
	e->binary.l = l;
	e->binary.r = r;

	return e;
}

static struct type *
inttype(uint64_t val, bool decimal, char *end)
{
	static struct {
		struct type *type;
		const char *end1, *end2;
	} limits[] = {
		{&typeint,    "",    NULL},
		{&typeuint,   "u",   NULL},
		{&typelong,   "l",   NULL},
		{&typeulong,  "ul",  "lu"},
		{&typellong,  "ll",  NULL},
		{&typeullong, "ull", "llu"},
	};
	struct type *t;
	size_t i, step;

	for (i = 0; end[i]; ++i)
		end[i] = tolower(end[i]);
	for (i = 0; i < LEN(limits); ++i) {
		if (strcmp(end, limits[i].end1) == 0)
			break;
		if (limits[i].end2 && strcmp(end, limits[i].end2) == 0)
			break;
	}
	if (i == LEN(limits))
		error(&tok.loc, "invalid integer constant suffix '%s'", end);
	step = i % 2 || decimal ? 2 : 1;
	for (; i < LEN(limits); i += step) {
		t = limits[i].type;
		if (val <= (uint64_t)-1 >> (8 - t->size << 3) + t->basic.issigned)
			return t;
	}
	error(&tok.loc, "no suitable type for constant '%s'", tok.lit);
}

static int
isodigit(int c)
{
	return '0' <= c && c <= '8';
}

static int
unescape(char **p)
{
	int c;
	char *s = *p;

	if (*s == '\\') {
		++s;
		switch (*s) {
		case '\'':
		case '"':
		case '?':
		case '\\': c = *s;   ++s; break;
		case 'a':  c = '\a'; ++s; break;
		case 'b':  c = '\b'; ++s; break;
		case 'f':  c = '\f'; ++s; break;
		case 'n':  c = '\n'; ++s; break;
		case 'r':  c = '\r'; ++s; break;
		case 't':  c = '\t'; ++s; break;
		case 'v':  c = '\v'; ++s; break;
		case 'x':
			++s;
			assert(isxdigit(*s));
			c = 0;
			do c = c * 16 + (*s > '9' ? 10 + tolower(*s) - 'a' : *s - '0');
			while (isxdigit(*++s));
			break;
		default:
			assert(isodigit(*s));
			c = 0;
			do c = c * 8 + (*s - '0');
			while (isodigit(*++s));
		}
	} else {
		c = *s++;
	}
	*p = s;
	return c;
}

/* 6.5 Expressions */
static struct expr *
primaryexpr(struct scope *s)
{
	struct expr *e;
	struct decl *d;
	char *src, *dst, *end;
	int base;

	switch (tok.kind) {
	case TIDENT:
		d = scopegetdecl(s, tok.lit, 1);
		if (!d)
			error(&tok.loc, "undeclared identifier: %s", tok.lit);
		e = mkexpr(EXPRIDENT, d->type, d->kind == DECLOBJECT ? EXPRFLAG_LVAL : 0);
		e->ident.decl = d;
		if (d->kind != DECLBUILTIN)
			e = decay(e);
		next();
		break;
	case TSTRINGLIT:
		e = mkexpr(EXPRSTRING, mkarraytype(&typechar, 0), EXPRFLAG_LVAL);
		e->string.size = 0;
		e->string.data = NULL;
		do {
			e->string.data = xreallocarray(e->string.data, e->string.size + strlen(tok.lit), 1);
			dst = e->string.data + e->string.size;
			src = tok.lit;
			if (*src != '"')
				fatal("wide string literal not yet implemented");
			for (++src; *src != '"'; ++dst)
				*dst = unescape(&src);
			e->string.size = dst - e->string.data;
			next();
		} while (tok.kind == TSTRINGLIT);
		e->type->array.length = e->string.size + 1;
		e->type->size = e->type->array.length * e->type->base->size;
		e = decay(e);
		break;
	case TCHARCONST:
		src = tok.lit;
		if (*src != '\'')
			fatal("wide character constant not yet implemented");
		++src;
		e = mkconstexpr(&typeint, unescape(&src));
		if (*src != '\'')
			error(&tok.loc, "character constant contains more than one character: %c", *src);
		next();
		break;
	case TNUMBER:
		e = mkexpr(EXPRCONST, NULL, 0);
		base = tok.lit[0] != '0' ? 10 : tolower(tok.lit[1]) == 'x' ? 16 : 8;
		if (strpbrk(tok.lit, base == 16 ? ".pP" : ".eE")) {
			/* floating constant */
			errno = 0;
			e->constant.f = strtod(tok.lit, &end);
			if (errno && errno != ERANGE)
				error(&tok.loc, "invalid floating constant '%s': %s", tok.lit, strerror(errno));
			if (!end[0])
				e->type = &typedouble;
			else if (tolower(end[0]) == 'f' && !end[1])
				e->type = &typefloat;
			else if (tolower(end[0]) == 'l' && !end[1])
				e->type = &typelongdouble;
			else
				error(&tok.loc, "invalid floating constant suffix '%s'", *end);
		} else {
			/* integer constant */
			errno = 0;
			e->constant.i = strtoull(tok.lit, &end, 0);
			if (errno)
				error(&tok.loc, "invalid integer constant '%s': %s", tok.lit, strerror(errno));
			e->type = inttype(e->constant.i, base == 10, end);
		}
		next();
		break;
	case TLPAREN:
		next();
		e = expr(s);
		expect(TRPAREN, "after expression");
		break;
	case T_GENERIC:
		error(&tok.loc, "generic selection is not yet supported");
	default:
		error(&tok.loc, "expected primary expression");
	}

	return e;
}

static struct expr *condexpr(struct scope *);

static struct expr *
builtinfunc(struct scope *s, enum builtinkind kind)
{
	struct expr *e;
	struct type *t;
	char *name;
	uint64_t offset;

	switch (kind) {
	case BUILTINALLOCA:
		e = mkexpr(EXPRBUILTIN, mkpointertype(&typevoid), 0);
		e->builtin.kind = BUILTINALLOCA;
		e->builtin.arg = exprconvert(assignexpr(s), &typeulong);
		break;
	case BUILTINCONSTANTP:
		e = mkconstexpr(&typeint, eval(condexpr(s))->kind == EXPRCONST);
		break;
	case BUILTININFF:
		e = mkexpr(EXPRCONST, &typefloat, 0);
		/* TODO: use INFINITY here when we can handle musl's math.h */
		e->constant.f = strtod("inf", NULL);
		break;
	case BUILTINNANF:
		e = assignexpr(s);
		if (!(e->flags & EXPRFLAG_DECAYED) || e->unary.base->kind != EXPRSTRING || e->unary.base->string.size > 0)
			error(&tok.loc, "__builtin_nanf currently only supports empty string literals");
		e = mkexpr(EXPRCONST, &typefloat, 0);
		/* TODO: use NAN here when we can handle musl's math.h */
		e->constant.f = strtod("nan", NULL);
		break;
	case BUILTINOFFSETOF:
		t = typename(s);
		expect(TCOMMA, "after type name");
		name = expect(TIDENT, "after ','");
		if (t->kind != TYPESTRUCT && t->kind != TYPEUNION)
			error(&tok.loc, "type is not a struct/union type");
		offset = 0;
		if (!typemember(t, name, &offset))
			error(&tok.loc, "struct/union has no member named '%s'", name);
		e = mkconstexpr(&typeulong, offset);
		free(name);
		break;
	case BUILTINVAARG:
		e = mkexpr(EXPRBUILTIN, NULL, 0);
		e->builtin.kind = BUILTINVAARG;
		e->builtin.arg = exprconvert(assignexpr(s), &typevalistptr);
		expect(TCOMMA, "after va_list");
		e->type = typename(s);
		break;
	case BUILTINVACOPY:
		e = mkexpr(EXPRASSIGN, typevalist.base, 0);
		e->assign.l = mkunaryexpr(TMUL, exprconvert(assignexpr(s), &typevalistptr));
		expect(TCOMMA, "after target va_list");
		e->assign.r = mkunaryexpr(TMUL, exprconvert(assignexpr(s), &typevalistptr));
		e = exprconvert(e, &typevoid);
		break;
	case BUILTINVAEND:
		e = mkexpr(EXPRBUILTIN, &typevoid, 0);
		e->builtin.kind = BUILTINVAEND;
		exprconvert(assignexpr(s), &typevalistptr);
		break;
	case BUILTINVASTART:
		e = mkexpr(EXPRBUILTIN, &typevoid, 0);
		e->builtin.kind = BUILTINVASTART;
		e->builtin.arg = exprconvert(assignexpr(s), &typevalistptr);
		expect(TCOMMA, "after va_list");
		free(expect(TIDENT, "after ','"));
		// XXX: check that this was actually a parameter name?
		break;
	default:
		fatal("internal error; unknown builtin");
	}
	return e;
}

static struct expr *
postfixexpr(struct scope *s, struct expr *r)
{
	struct expr *e, *arr, *idx, *tmp, **end;
	struct type *t;
	enum typequal tq;
	struct param *p;
	uint64_t offset;
	enum tokenkind op;
	bool lvalue;

	if (!r)
		r = primaryexpr(s);
	for (;;) {
		switch (tok.kind) {
		case TLBRACK:  /* subscript */
			next();
			arr = r;
			idx = expr(s);
			lvalueconvert(arr);
			lvalueconvert(idx);
			if (arr->type->kind != TYPEPOINTER) {
				if (idx->type->kind != TYPEPOINTER)
					error(&tok.loc, "either array or index must be pointer type");
				tmp = arr;
				arr = idx;
				idx = tmp;
			}
			if (arr->type->base->incomplete)
				error(&tok.loc, "array is pointer to incomplete type");
			if (!(typeprop(idx->type) & PROPINT))
				error(&tok.loc, "index is not an integer type");
			e = mkunaryexpr(TMUL, mkbinaryexpr(&tok.loc, TADD, arr, idx));
			expect(TRBRACK, "after array index");
			break;
		case TLPAREN:  /* function call */
			next();
			if (r->kind == EXPRIDENT && r->ident.decl->kind == DECLBUILTIN) {
				e = builtinfunc(s, r->ident.decl->builtin);
				expect(TRPAREN, "after builtin parameters");
				break;
			}
			lvalueconvert(r);
			if (r->type->kind != TYPEPOINTER || r->type->base->kind != TYPEFUNC)
				error(&tok.loc, "called object is not a function");
			t = r->type->base;
			e = mkexpr(EXPRCALL, t->base, 0);
			e->call.func = r;
			e->call.args = NULL;
			e->call.nargs = 0;
			p = t->func.params;
			end = &e->call.args;
			for (;;) {
				if (tok.kind == TRPAREN)
					break;
				if (e->call.args)
					expect(TCOMMA, "or ')' after function call argument");
				if (!p && !t->func.isvararg && t->func.paraminfo)
					error(&tok.loc, "too many arguments for function call");
				*end = assignexpr(s);
				lvalueconvert(*end);
				if (!t->func.isprototype || (t->func.isvararg && !p))
					*end = exprconvert(*end, typeargpromote((*end)->type));
				else
					*end = exprconvert(*end, p->type);
				end = &(*end)->next;
				++e->call.nargs;
				if (p)
					p = p->next;
			}
			if (!t->func.isprototype && p)
				error(&tok.loc, "not enough arguments for function call");
			e = decay(e);
			next();
			break;
		case TPERIOD:
			r = mkunaryexpr(TBAND, r);
			/* fallthrough */
		case TARROW:
			op = tok.kind;
			lvalueconvert(r);
			if (r->type->kind != TYPEPOINTER)
				error(&tok.loc, "arrow operator must be applied to pointer to struct/union");
			tq = QUALNONE;
			t = typeunqual(r->type->base, &tq);
			if (t->kind != TYPESTRUCT && t->kind != TYPEUNION)
				error(&tok.loc, "arrow operator must be applied to pointer to struct/union");
			next();
			if (tok.kind != TIDENT)
				error(&tok.loc, "expected identifier after '->' operator");
			lvalue = op == TARROW || r->unary.base->flags & EXPRFLAG_LVAL;
			r = exprconvert(r, mkpointertype(&typechar));
			offset = 0;
			t = typemember(t, tok.lit, &offset);
			if (!t)
				error(&tok.loc, "struct/union has no member named '%s'", tok.lit);
			r = mkbinaryexpr(&tok.loc, TADD, r, mkconstexpr(&typeulong, offset));
			r = exprconvert(r, mkpointertype(mkqualifiedtype(t, tq)));
			e = mkunaryexpr(TMUL, r);
			if (!lvalue)
				e->flags &= ~EXPRFLAG_LVAL;
			next();
			break;
		case TINC:
		case TDEC:
			e = mkexpr(EXPRINCDEC, r->type, 0);
			e->incdec.op = tok.kind;
			e->incdec.base = r;
			e->incdec.post = 1;
			next();
			break;
		default:
			return r;
		}
		r = e;
	}
}

static struct expr *castexpr(struct scope *);

static struct expr *
unaryexpr(struct scope *s)
{
	enum tokenkind op;
	struct expr *e, *l;
	struct type *t;

	op = tok.kind;
	switch (op) {
	case TINC:
	case TDEC:
		next();
		l = unaryexpr(s);
		if (!(l->flags & EXPRFLAG_LVAL))
			error(&tok.loc, "operand of %srement operator must be an lvalue", op == TINC ? "inc" : "dec");
		/*
		if (l->qualifiers & QUALCONST)
			error(&tok.loc, "operand of %srement operator is const qualified", op == TINC ? "inc" : "dec");
		*/
		e = mkexpr(EXPRINCDEC, l->type, 0);
		e->incdec.op = op;
		e->incdec.base = l;
		e->incdec.post = 0;
		break;
	case TBAND:
	case TMUL:
		next();
		return mkunaryexpr(op, castexpr(s));
	case TADD:
		next();
		e = castexpr(s);
		lvalueconvert(e);
		e = exprconvert(e, typeintpromote(e->type));
		break;
	case TSUB:
		next();
		e = castexpr(s);
		lvalueconvert(e);
		e = exprconvert(e, typeintpromote(e->type));
		e = mkbinaryexpr(&tok.loc, TSUB, mkconstexpr(&typeint, 0), e);
		break;
	case TBNOT:
		next();
		e = castexpr(s);
		lvalueconvert(e);
		e = exprconvert(e, typeintpromote(e->type));
		e = mkbinaryexpr(&tok.loc, TXOR, e, mkconstexpr(e->type, -1));
		break;
	case TLNOT:
		next();
		e = castexpr(s);
		lvalueconvert(e);
		if (!(typeprop(e->type) & PROPSCALAR))
			error(&tok.loc, "operator '!' must have scalar operand");
		e = mkbinaryexpr(&tok.loc, TEQL, e, mkconstexpr(&typeint, 0));
		break;
	case TSIZEOF:
	case T_ALIGNOF:
		next();
		if (consume(TLPAREN)) {
			t = typename(s);
			if (t) {
				expect(TRPAREN, "after type name");
				/* might be part of a compound literal */
				if (op == TSIZEOF && tok.kind == TLBRACE)
					parseinit(s, t);
			} else {
				e = expr(s);
				expect(TRPAREN, "after expression");
				if (op == TSIZEOF)
					e = postfixexpr(s, e);
				if (e->flags & EXPRFLAG_DECAYED)
					e = e->unary.base;
				t = e->type;
			}
		} else if (op == TSIZEOF) {
			e = unaryexpr(s);
			if (e->flags & EXPRFLAG_DECAYED)
				e = e->unary.base;
			t = e->type;
		} else {
			error(&tok.loc, "expected ')' after '_Alignof'");
		}
		e = mkconstexpr(&typeulong, op == TSIZEOF ? t->size : t->align);
		break;
	default:
		e = postfixexpr(s, NULL);
	}

	return e;
}

static struct expr *
castexpr(struct scope *s)
{
	struct type *t;
	struct expr *r, *e, **end;

	end = &r;
	while (consume(TLPAREN)) {
		t = typename(s);
		if (!t) {
			e = expr(s);
			expect(TRPAREN, "after expression to match '('");
			*end = postfixexpr(s, e);
			return r;
		}
		expect(TRPAREN, "after type name");
		if (tok.kind == TLBRACE) {
			e = mkexpr(EXPRCOMPOUND, t, EXPRFLAG_LVAL);
			e->compound.init = parseinit(s, t);
			e = decay(e);
			*end = postfixexpr(s, e);
			return r;
		}
		e = mkexpr(EXPRCAST, t, 0);
		// XXX check types 6.5.4
		*end = e;
		end = &e->cast.e;
	}
	*end = unaryexpr(s);

	return r;
}

static int
precedence(enum tokenkind t)
{
	switch (t) {
	case TLOR:     return 0;
	case TLAND:    return 1;
	case TBOR:     return 2;
	case TXOR:     return 3;
	case TBAND:    return 4;
	case TEQL:
	case TNEQ:     return 5;
	case TLESS:
	case TGREATER:
	case TLEQ:
	case TGEQ:     return 6;
	case TSHL:
	case TSHR:     return 7;
	case TADD:
	case TSUB:     return 8;
	case TMUL:
	case TDIV:
	case TMOD:     return 9;
	}
	return -1;
}

static struct expr *
binaryexpr(struct scope *s, struct expr *l, int i)
{
	struct expr *r;
	struct location loc;
	enum tokenkind op;
	int j, k;

	if (!l)
		l = castexpr(s);
	while ((j = precedence(tok.kind)) >= i) {
		op = tok.kind;
		loc = tok.loc;
		next();
		r = castexpr(s);
		while ((k = precedence(tok.kind)) > j)
			r = binaryexpr(s, r, k);
		l = mkbinaryexpr(&loc, op, l, r);
	}
	return l;
}

static bool
nullpointer(struct expr *e)
{
	if (e->kind != EXPRCONST)
		return false;
	if (!(typeprop(e->type) & PROPINT) && (e->type->kind != TYPEPOINTER || e->type->base != &typevoid))
		return false;
	return e->constant.i == 0;
}

static struct expr *
condexpr(struct scope *s)
{
	struct expr *r, *e;
	struct type *t, *f;
	enum typequal tq;

	r = binaryexpr(s, NULL, 0);
	if (!consume(TQUESTION))
		return r;
	e = mkexpr(EXPRCOND, NULL, 0);
	e->cond.e = exprconvert(r, &typebool);
	e->cond.t = expr(s);
	expect(TCOLON, "in conditional expression");
	e->cond.f = condexpr(s);
	lvalueconvert(e->cond.t);
	lvalueconvert(e->cond.f);
	t = e->cond.t->type;
	f = e->cond.f->type;
	if (t == f) {
		e->type = t;
	} else if (typeprop(t) & PROPARITH && typeprop(f) & PROPARITH) {
		e->type = commonreal(&e->cond.t, &e->cond.f);
	} else if (t == &typevoid && f == &typevoid) {
		e->type = &typevoid;
	} else {
		e->cond.t = eval(e->cond.t);
		e->cond.f = eval(e->cond.f);
		if (nullpointer(e->cond.t) && f->kind == TYPEPOINTER) {
			e->type = f;
		} else if (nullpointer(e->cond.f) && t->kind == TYPEPOINTER) {
			e->type = t;
		} else if (t->kind == TYPEPOINTER && f->kind == TYPEPOINTER) {
			tq = QUALNONE;
			t = typeunqual(t->base, &tq);
			f = typeunqual(f->base, &tq);
			if (t == &typevoid || f == &typevoid) {
				e->type = &typevoid;
			} else {
				if (!typecompatible(t, f))
					error(&tok.loc, "operands of conditional operator must have compatible types");
				e->type = typecomposite(t, f);
			}
			e->type = mkpointertype(mkqualifiedtype(e->type, tq));
		} else {
			error(&tok.loc, "invalid operands to conditional operator");
		}
	}

	return e;
}

uint64_t
intconstexpr(struct scope *s, bool allowneg)
{
	struct expr *e;

	e = eval(condexpr(s));
	if (e->kind != EXPRCONST || !(typeprop(e->type) & PROPINT))
		error(&tok.loc, "not an integer constant expression");
	if (!allowneg && e->type->basic.issigned && e->constant.i > INT64_MAX)
		error(&tok.loc, "integer constant expression cannot be negative");
	return e->constant.i;
}

struct expr *
assignexpr(struct scope *s)
{
	struct expr *e, *l, *r, *tmp = NULL, **res = &e;
	enum tokenkind op;

	l = condexpr(s);
	if (l->kind == EXPRBINARY || l->kind == EXPRCOMMA || l->kind == EXPRCAST)
		return l;
	switch (tok.kind) {
	case TASSIGN: op = TNONE; break;
	case TMULASSIGN: op = TMUL; break;
	case TDIVASSIGN: op = TDIV; break;
	case TMODASSIGN: op = TMOD; break;
	case TADDASSIGN: op = TADD; break;
	case TSUBASSIGN: op = TSUB; break;
	case TSHLASSIGN: op = TSHL; break;
	case TSHRASSIGN: op = TSHR; break;
	case TBANDASSIGN: op = TBAND; break;
	case TXORASSIGN: op = TXOR; break;
	case TBORASSIGN: op = TBOR; break;
	default:
		return l;
	}
	if (!(l->flags & EXPRFLAG_LVAL))
		error(&tok.loc, "left side of assignment expression is not an lvalue");
	next();
	r = assignexpr(s);
	lvalueconvert(r);
	if (op) {
		/* rewrite `E1 OP= E2` as `T = &E1, *T = *T OP E2`, where T is a temporary slot */
		tmp = mkexpr(EXPRTEMP, mkpointertype(l->type), EXPRFLAG_LVAL);
		tmp->temp = NULL;
		e = mkexpr(EXPRCOMMA, l->type, 0);
		e->comma.exprs = mkexpr(EXPRASSIGN, tmp->type, 0);
		e->comma.exprs->assign.l = tmp;
		e->comma.exprs->assign.r = mkunaryexpr(TBAND, l);
		res = &e->comma.exprs->next;
		l = mkunaryexpr(TMUL, tmp);
		r = mkbinaryexpr(&tok.loc, op, l, r);
	}
	r = exprconvert(r, l->type);
	*res = mkexpr(EXPRASSIGN, l->type, 0);
	(*res)->assign.l = l;
	(*res)->assign.r = r;

	return e;
}

struct expr *
expr(struct scope *s)
{
	struct expr *r, *e, **end;

	end = &r;
	for (;;) {
		e = assignexpr(s);
		*end = e;
		end = &e->next;
		if (tok.kind != TCOMMA)
			break;
		next();
	}
	if (!r->next)
		return r;
	e = mkexpr(EXPRCOMMA, e->type, 0);
	e->comma.exprs = r;

	return e;
}

struct expr *
exprconvert(struct expr *e, struct type *t)
{
	struct expr *cast;

	if (typecompatible(e->type, t))
		return e;
	cast = mkexpr(EXPRCAST, t, 0);
	cast->cast.e = e;

	return cast;
}
