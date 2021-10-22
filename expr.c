#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "cc.h"

static struct expr *
mkexpr(enum exprkind k, struct type *t, struct expr *b)
{
	struct expr *e;

	e = xmalloc(sizeof(*e));
	e->qual = QUALNONE;
	e->type = t;
	e->lvalue = false;
	e->decayed = false;
	e->kind = k;
	e->base = b;
	e->next = NULL;

	return e;
}

void
delexpr(struct expr *e)
{
	struct expr *sub;

	switch (e->kind) {
	case EXPRCALL:
		delexpr(e->base);
		while (sub = e->call.args) {
			e->call.args = sub->next;
			delexpr(sub);
		}
		break;
	case EXPRBITFIELD:
	case EXPRINCDEC:
	case EXPRUNARY:
	case EXPRCAST:
		delexpr(e->base);
		break;
	case EXPRBINARY:
		delexpr(e->binary.l);
		delexpr(e->binary.r);
		break;
	case EXPRCOND:
		delexpr(e->base);
		delexpr(e->cond.t);
		delexpr(e->cond.f);
		break;
	/*
	XXX: compound assignment causes some reuse of expressions,
	so we can't free them without risk of a double-free

	case EXPRASSIGN:
		delexpr(e->assign.l);
		delexpr(e->assign.r);
		break;
	*/
	case EXPRCOMMA:
		while (sub = e->base) {
			e->base = sub->next;
			delexpr(sub);
		}
		break;
	}
	free(e);
}

static struct expr *
mkconstexpr(struct type *t, uint64_t n)
{
	struct expr *e;

	e = mkexpr(EXPRCONST, t, NULL);
	e->constant.u = n;

	return e;
}

static struct expr *mkunaryexpr(enum tokenkind, struct expr *);

/* 6.3.2.1 Conversion of arrays and function designators */
static struct expr *
decay(struct expr *e)
{
	struct type *t;
	enum typequal tq;

	// XXX: combine with decl.c:adjust in some way?
	t = e->type;
	tq = e->qual;
	switch (t->kind) {
	case TYPEARRAY:
		e = mkunaryexpr(TBAND, e);
		e->type = mkpointertype(t->base, tq);
		e->decayed = true;
		break;
	case TYPEFUNC:
		e = mkunaryexpr(TBAND, e);
		e->decayed = true;
		break;
	}

	return e;
}

static struct expr *
mkunaryexpr(enum tokenkind op, struct expr *base)
{
	struct expr *expr;

	switch (op) {
	case TBAND:
		if (base->decayed) {
			expr = base;
			base = base->base;
			free(expr);
		}
		/*
		Allow struct and union types even if they are not lvalues,
		since we take their address when compiling member access.
		*/
		if (!base->lvalue && base->type->kind != TYPEFUNC && base->type->kind != TYPESTRUCT && base->type->kind != TYPEUNION)
			error(&tok.loc, "'&' operand is not an lvalue or function designator");
		if (base->kind == EXPRBITFIELD)
			error(&tok.loc, "cannot take address of bit-field");
		expr = mkexpr(EXPRUNARY, mkpointertype(base->type, base->qual), base);
		expr->op = op;
		return expr;
	case TMUL:
		if (base->type->kind != TYPEPOINTER)
			error(&tok.loc, "cannot dereference non-pointer");
		expr = mkexpr(EXPRUNARY, base->type->base, base);
		expr->qual = base->type->qual;
		expr->lvalue = true;
		expr->op = op;
		return decay(expr);
	}
	/* other unary operators get compiled as equivalent binary ones */
	fatal("internal error: unknown unary operator %d", op);
}

static unsigned
bitfieldwidth(struct expr *e)
{
	if (e->kind != EXPRBITFIELD)
		return -1;
	return e->type->size * 8 - e->bitfield.bits.before - e->bitfield.bits.after;
}

struct expr *
exprpromote(struct expr *e)
{
	struct type *t;

	t = typepromote(e->type, bitfieldwidth(e));
	return exprconvert(e, t);
}

static struct type *
commonreal(struct expr **e1, struct expr **e2)
{
	struct type *t;

	t = typecommonreal((*e1)->type, bitfieldwidth(*e1), (*e2)->type, bitfieldwidth(*e2));
	*e1 = exprconvert(*e1, t);
	*e2 = exprconvert(*e2, t);

	return t;
}

static bool
nullpointer(struct expr *e)
{
	if (e->kind != EXPRCONST)
		return false;
	if (!(e->type->prop & PROPINT) && (e->type->kind != TYPEPOINTER || e->type->base != &typevoid))
		return false;
	return e->constant.u == 0;
}

static struct expr *
mkbinaryexpr(struct location *loc, enum tokenkind op, struct expr *l, struct expr *r)
{
	struct expr *e;
	struct type *t = NULL;
	enum typeprop lp, rp;

	lp = l->type->prop;
	rp = r->type->prop;
	switch (op) {
	case TLOR:
	case TLAND:
		if (!(lp & PROPSCALAR))
			error(loc, "left operand of '%s' operator must be scalar", tokstr[op]);
		if (!(rp & PROPSCALAR))
			error(loc, "right operand of '%s' operator must be scalar", tokstr[op]);
		t = &typeint;
		break;
	case TEQL:
	case TNEQ:
		t = &typeint;
		if (lp & PROPARITH && rp & PROPARITH) {
			commonreal(&l, &r);
			break;
		}
		if (l->type->kind != TYPEPOINTER)
			e = l, l = r, r = e;
		if (l->type->kind != TYPEPOINTER)
			error(loc, "invalid operands to '%s' operator", tokstr[op]);
		if (nullpointer(eval(r, EVALARITH))) {
			r = exprconvert(r, l->type);
			break;
		}
		if (nullpointer(eval(l, EVALARITH))) {
			l = exprconvert(l, r->type);
			break;
		}
		if (r->type->kind != TYPEPOINTER)
			error(loc, "invalid operands to '%s' operator", tokstr[op]);
		if (l->type->base->kind == TYPEVOID)
			e = l, l = r, r = e;
		if (r->type->base->kind == TYPEVOID && l->type->base->kind != TYPEFUNC)
			r = exprconvert(r, l->type);
		else if (!typecompatible(l->type->base, r->type->base))
			error(loc, "pointer operands to '%s' operator are to incompatible types", tokstr[op]);
		break;
	case TLESS:
	case TGREATER:
	case TLEQ:
	case TGEQ:
		t = &typeint;
		if (lp & PROPREAL && rp & PROPREAL) {
			commonreal(&l, &r);
		} else if (l->type->kind == TYPEPOINTER && r->type->kind == TYPEPOINTER) {
			if (!typecompatible(l->type->base, r->type->base) || l->type->base->kind == TYPEFUNC)
				error(loc, "pointer operands to '%s' operator must be to compatible object types", tokstr[op]);
		} else {
			error(loc, "invalid operands to '%s' operator", tokstr[op]);
		}
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
		if (t->base->incomplete || !(t->base->prop & PROPOBJECT))
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
		if (l->type->base->incomplete || !(l->type->base->prop & PROPOBJECT))
			error(loc, "pointer operand to '-' must be to complete object type");
		if (rp & PROPINT) {
			t = l->type;
			r = mkbinaryexpr(loc, TMUL, exprconvert(r, &typeulong), mkconstexpr(&typeulong, t->base->size));
		} else {
			if (!typecompatible(l->type->base, r->type->base))
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
			error(loc, "operands to '%s' operator must be arithmetic", tokstr[op]);
		t = commonreal(&l, &r);
		break;
	case TSHL:
	case TSHR:
		if (!(lp & PROPINT) || !(rp & PROPINT))
			error(loc, "operands to '%s' operator must be integer", tokstr[op]);
		l = exprpromote(l);
		r = exprpromote(r);
		t = l->type;
		break;
	default:
		fatal("internal error: unknown binary operator %d", op);
	}
	e = mkexpr(EXPRBINARY, t, NULL);
	e->op = op;
	e->binary.l = l;
	e->binary.r = r;

	return e;
}

static struct type *
inttype(unsigned long long val, bool decimal, char *end)
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
		if (val <= 0xffffffffffffffffu >> (8 - t->size << 3) + t->basic.issigned)
			return t;
	}
	error(&tok.loc, "no suitable type for constant '%s'", tok.lit);
}

static int
isodigit(int c)
{
	return '0' <= c && c <= '8';
}

unsigned
unescape(char **p)
{
	unsigned c;
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
		c = (unsigned char)*s++;
	}
	*p = s;
	return c;
}

static struct expr *
generic(struct scope *s)
{
	struct expr *e, *match = NULL, *def = NULL;
	struct type *t, *want;
	enum typequal qual;

	next();
	expect(TLPAREN, "after '_Generic'");
	e = assignexpr(s);
	expect(TCOMMA, "after generic selector expression");
	want = e->type;
	delexpr(e);
	do {
		if (consume(TDEFAULT)) {
			if (def)
				error(&tok.loc, "multiple default expressions in generic association list");
			expect(TCOLON, "after 'default'");
			def = assignexpr(s);
		} else {
			qual = QUALNONE;
			t = typename(s, &qual);
			if (!t)
				error(&tok.loc, "expected typename for generic association");
			expect(TCOLON, "after type name");
			e = assignexpr(s);
			if (typecompatible(t, want) && qual == QUALNONE) {
				if (match)
					error(&tok.loc, "generic selector matches multiple associations");
				match = e;
			} else {
				delexpr(e);
			}
		}
	} while (consume(TCOMMA));
	expect(TRPAREN, "after generic assocation list");
	if (!match) {
		if (!def)
			error(&tok.loc, "generic selector matches no associations and no default was specified");
		match = def;
	} else if (def) {
		delexpr(def);
	}
	return match;
}

/* 6.5 Expressions */
static struct expr *
primaryexpr(struct scope *s)
{
	struct expr *e;
	struct decl *d;
	struct type *t;
	char *src, *end;
	unsigned char *dst;
	int base;

	switch (tok.kind) {
	case TIDENT:
		d = scopegetdecl(s, tok.lit, 1);
		if (!d)
			error(&tok.loc, "undeclared identifier: %s", tok.lit);
		e = mkexpr(EXPRIDENT, d->type, NULL);
		e->qual = d->qual;
		e->lvalue = d->kind == DECLOBJECT;
		e->ident.decl = d;
		if (d->kind != DECLBUILTIN)
			e = decay(e);
		next();
		break;
	case TSTRINGLIT:
		e = mkexpr(EXPRSTRING, mkarraytype(&typechar, QUALNONE, 0), NULL);
		e->lvalue = true;
		e->string.size = 0;
		e->string.data = NULL;
		do {
			e->string.data = xreallocarray(e->string.data, e->string.size + strlen(tok.lit) + 1, 1);
			dst = e->string.data + e->string.size;
			src = tok.lit;
			if (*src != '"')
				fatal("wide string literal not yet implemented");
			for (++src; *src != '"'; ++dst)
				*dst = unescape(&src);
			e->string.size = dst - e->string.data;
			next();
		} while (tok.kind == TSTRINGLIT);
		*dst = '\0';
		e->type->array.length = ++e->string.size;
		e->type->size = e->type->array.length * e->type->base->size;
		e->type->incomplete = false;
		e = decay(e);
		break;
	case TCHARCONST:
		src = tok.lit;
		t = &typeint;
		switch (*src) {
		case 'L': ++src; t = targ->typewchar; break;
		case 'u': ++src; t = &typeushort;     break;
		case 'U': ++src; t = &typeuint;       break;
		}
		assert(*src == '\'');
		++src;
		e = mkconstexpr(t, unescape(&src));
		if (*src != '\'')
			error(&tok.loc, "character constant contains more than one character: %c", *src);
		next();
		break;
	case TNUMBER:
		e = mkexpr(EXPRCONST, NULL, NULL);
		base = tok.lit[0] != '0' ? 10 : tolower(tok.lit[1]) == 'x' ? 16 : 8;
		if (strpbrk(tok.lit, base == 16 ? ".pP" : ".eE")) {
			/* floating constant */
			e->constant.f = strtod(tok.lit, &end);
			if (end == tok.lit)
				error(&tok.loc, "invalid floating constant '%s'", tok.lit);
			if (!end[0])
				e->type = &typedouble;
			else if (tolower(end[0]) == 'f' && !end[1])
				e->type = &typefloat;
			else if (tolower(end[0]) == 'l' && !end[1])
				e->type = &typeldouble;
			else
				error(&tok.loc, "invalid floating constant suffix '%s'", end);
		} else {
			/* integer constant */
			e->constant.u = strtoull(tok.lit, &end, 0);
			if (end == tok.lit)
				error(&tok.loc, "invalid integer constant '%s'", tok.lit);
			e->type = inttype(e->constant.u, base == 10, end);
		}
		next();
		break;
	case TLPAREN:
		next();
		e = expr(s);
		expect(TRPAREN, "after expression");
		break;
	case T_GENERIC:
		e = generic(s);
		break;
	default:
		error(&tok.loc, "expected primary expression");
	}

	return e;
}

static struct expr *condexpr(struct scope *);

/* TODO: merge with init.c:designator() */
static void
designator(struct scope *s, struct type *t, unsigned long long *offset)
{
	char *name;
	struct member *m;
	uint64_t i;

	for (;;) {
		switch (tok.kind) {
		case TLBRACK:
			if (t->kind != TYPEARRAY)
				error(&tok.loc, "index designator is only valid for array types");
			next();
			i = intconstexpr(s, false);
			expect(TRBRACK, "for index designator");
			t = t->base;
			*offset += i * t->size;
			break;
		case TPERIOD:
			if (t->kind != TYPESTRUCT && t->kind != TYPEUNION)
				error(&tok.loc, "member designator only valid for struct/union types");
			next();
			name = expect(TIDENT, "for member designator");
			m = typemember(t, name, offset);
			if (!m)
				error(&tok.loc, "%s has no member named '%s'", t->kind == TYPEUNION ? "union" : "struct", name);
			free(name);
			t = m->type;
			break;
		default:
			return;
		}
	}
}

static struct expr *
builtinfunc(struct scope *s, enum builtinkind kind)
{
	struct expr *e, *param;
	struct type *t;
	struct member *m;
	char *name;
	unsigned long long offset;

	switch (kind) {
	case BUILTINALLOCA:
		e = exprconvert(assignexpr(s), &typeulong);
		e = mkexpr(EXPRBUILTIN, mkpointertype(&typevoid, QUALNONE), e);
		e->builtin.kind = BUILTINALLOCA;
		break;
	case BUILTINCONSTANTP:
		e = mkconstexpr(&typeint, eval(condexpr(s), EVALARITH)->kind == EXPRCONST);
		break;
	case BUILTINEXPECT:
		/* just a no-op for now */
		/* TODO: check that the expression and the expected value have type 'long' */
		e = assignexpr(s);
		expect(TCOMMA, "after expression");
		delexpr(assignexpr(s));
		break;
	case BUILTININFF:
		e = mkexpr(EXPRCONST, &typefloat, NULL);
		/* TODO: use INFINITY here when we can handle musl's math.h */
		e->constant.f = strtod("inf", NULL);
		break;
	case BUILTINNANF:
		e = assignexpr(s);
		if (!e->decayed || e->base->kind != EXPRSTRING || e->base->string.size > 1)
			error(&tok.loc, "__builtin_nanf currently only supports empty string literals");
		e = mkexpr(EXPRCONST, &typefloat, NULL);
		/* TODO: use NAN here when we can handle musl's math.h */
		e->constant.f = strtod("nan", NULL);
		break;
	case BUILTINOFFSETOF:
		t = typename(s, NULL);
		expect(TCOMMA, "after type name");
		name = expect(TIDENT, "after ','");
		if (t->kind != TYPESTRUCT && t->kind != TYPEUNION)
			error(&tok.loc, "type is not a struct/union type");
		offset = 0;
		m = typemember(t, name, &offset);
		if (!m)
			error(&tok.loc, "struct/union has no member named '%s'", name);
		designator(s, m->type, &offset);
		e = mkconstexpr(&typeulong, offset);
		free(name);
		break;
	case BUILTINTYPESCOMPATIBLEP:
		t = typename(s, NULL);
		expect(TCOMMA, "after type name");
		e = mkconstexpr(&typeint, typecompatible(t, typename(s, NULL)));
		break;
	case BUILTINVAARG:
		e = mkexpr(EXPRBUILTIN, NULL, assignexpr(s));
		e->builtin.kind = BUILTINVAARG;
		if (!typesame(e->base->type, typeadjvalist))
			error(&tok.loc, "va_arg argument must have type va_list");
		if (typeadjvalist == targ->typevalist)
			e->base = mkunaryexpr(TBAND, e->base);
		expect(TCOMMA, "after va_list");
		e->type = typename(s, &e->qual);
		break;
	case BUILTINVACOPY:
		e = mkexpr(EXPRASSIGN, &typevoid, NULL);
		e->assign.l = assignexpr(s);
		if (!typesame(e->assign.l->type, typeadjvalist))
			error(&tok.loc, "va_copy destination must have type va_list");
		if (typeadjvalist != targ->typevalist)
			e->assign.l = mkunaryexpr(TMUL, e->assign.l);
		expect(TCOMMA, "after target va_list");
		e->assign.r = assignexpr(s);
		if (!typesame(e->assign.r->type, typeadjvalist))
			error(&tok.loc, "va_copy source must have type va_list");
		if (typeadjvalist != targ->typevalist)
			e->assign.r = mkunaryexpr(TMUL, e->assign.r);
		break;
	case BUILTINVAEND:
		e = assignexpr(s);
		if (!typesame(e->type, typeadjvalist))
			error(&tok.loc, "va_end argument must have type va_list");
		e = mkexpr(EXPRBUILTIN, &typevoid, NULL);
		e->builtin.kind = BUILTINVAEND;
		break;
	case BUILTINVASTART:
		e = mkexpr(EXPRBUILTIN, &typevoid, assignexpr(s));
		e->builtin.kind = BUILTINVASTART;
		if (!typesame(e->base->type, typeadjvalist))
			error(&tok.loc, "va_start argument must have type va_list");
		if (typeadjvalist == targ->typevalist)
			e->base = mkunaryexpr(TBAND, e->base);
		expect(TCOMMA, "after va_list");
		param = assignexpr(s);
		if (param->kind != EXPRIDENT)
			error(&tok.loc, "expected parameter identifier");
		delexpr(param);
		// XXX: check that this was actually a parameter name?
		break;
	default:
		fatal("internal error; unknown builtin");
	}
	return e;
}

static struct expr *
mkincdecexpr(enum tokenkind op, struct expr *base, bool post)
{
	struct expr *e;

	if (!base->lvalue)
		error(&tok.loc, "operand of '%s' operator must be an lvalue", tokstr[op]);
	if (base->qual & QUALCONST)
		error(&tok.loc, "operand of '%s' operator is const qualified", tokstr[op]);
	e = mkexpr(EXPRINCDEC, base->type, base);
	e->op = op;
	e->incdec.post = post;
	return e;
}

static struct expr *
postfixexpr(struct scope *s, struct expr *r)
{
	struct expr *e, *arr, *idx, *tmp, **end;
	struct type *t;
	struct param *p;
	struct member *m;
	unsigned long long offset;
	enum typequal tq;
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
			if (arr->type->kind != TYPEPOINTER) {
				if (idx->type->kind != TYPEPOINTER)
					error(&tok.loc, "either array or index must be pointer type");
				tmp = arr;
				arr = idx;
				idx = tmp;
			}
			if (arr->type->base->incomplete)
				error(&tok.loc, "array is pointer to incomplete type");
			if (!(idx->type->prop & PROPINT))
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
			if (r->type->kind != TYPEPOINTER || r->type->base->kind != TYPEFUNC)
				error(&tok.loc, "called object is not a function");
			t = r->type->base;
			e = mkexpr(EXPRCALL, t->base, r);
			e->call.args = NULL;
			e->call.nargs = 0;
			p = t->func.params;
			end = &e->call.args;
			while (tok.kind != TRPAREN) {
				if (e->call.args)
					expect(TCOMMA, "or ')' after function call argument");
				if (!p && !t->func.isvararg && t->func.paraminfo)
					error(&tok.loc, "too many arguments for function call");
				*end = assignexpr(s);
				if (!t->func.isprototype || (t->func.isvararg && !p))
					*end = exprpromote(*end);
				else
					*end = exprconvert(*end, p->type);
				end = &(*end)->next;
				++e->call.nargs;
				if (p)
					p = p->next;
			}
			if (p && !t->func.isvararg && t->func.paraminfo)
				error(&tok.loc, "not enough arguments for function call");
			e = decay(e);
			next();
			break;
		case TPERIOD:
			r = mkunaryexpr(TBAND, r);
			/* fallthrough */
		case TARROW:
			op = tok.kind;
			if (r->type->kind != TYPEPOINTER)
				error(&tok.loc, "'%s' operator must be applied to pointer to struct/union", tokstr[op]);
			t = r->type->base;
			tq = r->type->qual;
			if (t->kind != TYPESTRUCT && t->kind != TYPEUNION)
				error(&tok.loc, "'%s' operator must be applied to pointer to struct/union", tokstr[op]);
			next();
			if (tok.kind != TIDENT)
				error(&tok.loc, "expected identifier after '%s' operator", tokstr[op]);
			lvalue = op == TARROW || r->base->lvalue;
			r = exprconvert(r, mkpointertype(&typechar, QUALNONE));
			offset = 0;
			m = typemember(t, tok.lit, &offset);
			if (!m)
				error(&tok.loc, "struct/union has no member named '%s'", tok.lit);
			r = mkbinaryexpr(&tok.loc, TADD, r, mkconstexpr(&typeulong, offset));
			r = exprconvert(r, mkpointertype(m->type, tq | m->qual));
			r = mkunaryexpr(TMUL, r);
			r->lvalue = lvalue;
			if (m->bits.before || m->bits.after) {
				e = mkexpr(EXPRBITFIELD, r->type, r);
				e->lvalue = lvalue;
				e->bitfield.bits = m->bits;
			} else {
				e = r;
			}
			next();
			break;
		case TINC:
		case TDEC:
			e = mkincdecexpr(tok.kind, r, true);
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
		e = mkincdecexpr(op, l, false);
		break;
	case TBAND:
	case TMUL:
		next();
		return mkunaryexpr(op, castexpr(s));
	case TADD:
		next();
		e = castexpr(s);
		if (!(e->type->prop & PROPARITH))
			error(&tok.loc, "operand of unary '+' operator must have arithmetic type");
		if (e->type->prop & PROPINT)
			e = exprpromote(e);
		break;
	case TSUB:
		next();
		e = castexpr(s);
		if (!(e->type->prop & PROPARITH))
			error(&tok.loc, "operand of unary '-' operator must have arithmetic type");
		if (e->type->prop & PROPINT)
			e = exprpromote(e);
		e = mkbinaryexpr(&tok.loc, TSUB, mkconstexpr(&typeint, 0), e);
		break;
	case TBNOT:
		next();
		e = castexpr(s);
		if (!(e->type->prop & PROPINT))
			error(&tok.loc, "operand of '~' operator must have integer type");
		e = exprpromote(e);
		e = mkbinaryexpr(&tok.loc, TXOR, e, mkconstexpr(e->type, -1));
		break;
	case TLNOT:
		next();
		e = castexpr(s);
		if (!(e->type->prop & PROPSCALAR))
			error(&tok.loc, "operator '!' must have scalar operand");
		e = mkbinaryexpr(&tok.loc, TEQL, e, mkconstexpr(&typeint, 0));
		break;
	case TSIZEOF:
	case T_ALIGNOF:
		next();
		if (consume(TLPAREN)) {
			t = typename(s, NULL);
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
			}
		} else if (op == TSIZEOF) {
			t = NULL;
			e = unaryexpr(s);
		} else {
			error(&tok.loc, "expected ')' after '_Alignof'");
		}
		if (!t) {
			if (e->decayed)
				e = e->base;
			if (e->kind == EXPRBITFIELD)
				error(&tok.loc, "%s operator applied to bitfield expression", tokstr[op]);
			t = e->type;
		}
		if (t->incomplete)
			error(&tok.loc, "%s operator applied to incomplete type", tokstr[op]);
		if (t->kind == TYPEFUNC)
			error(&tok.loc, "%s operator applied to function type", tokstr[op]);
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
	enum typequal tq;
	struct expr *r, *e, **end;

	end = &r;
	while (consume(TLPAREN)) {
		tq = QUALNONE;
		t = typename(s, &tq);
		if (!t) {
			e = expr(s);
			expect(TRPAREN, "after expression to match '('");
			*end = postfixexpr(s, e);
			return r;
		}
		expect(TRPAREN, "after type name");
		if (tok.kind == TLBRACE) {
			e = mkexpr(EXPRCOMPOUND, t, NULL);
			e->qual = tq;
			e->lvalue = true;
			e->compound.init = parseinit(s, t);
			e = decay(e);
			*end = postfixexpr(s, e);
			return r;
		}
		e = mkexpr(EXPRCAST, t, NULL);
		// XXX check types 6.5.4
		*end = e;
		end = &e->base;
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

static struct expr *
condexpr(struct scope *s)
{
	struct expr *e, *l, *r;
	struct type *t, *lt, *rt;
	enum typequal tq;

	e = binaryexpr(s, NULL, 0);
	if (!consume(TQUESTION))
		return e;
	l = expr(s);
	expect(TCOLON, "in conditional expression");
	r = condexpr(s);

	lt = l->type;
	rt = r->type;
	if (lt == rt) {
		t = lt;
	} else if (lt->prop & PROPARITH && rt->prop & PROPARITH) {
		t = commonreal(&l, &r);
	} else if (lt == &typevoid && rt == &typevoid) {
		t = &typevoid;
	} else {
		l = eval(l, EVALARITH);
		r = eval(r, EVALARITH);
		if (nullpointer(l) && rt->kind == TYPEPOINTER) {
			t = rt;
		} else if (nullpointer(r) && lt->kind == TYPEPOINTER) {
			t = lt;
		} else if (lt->kind == TYPEPOINTER && rt->kind == TYPEPOINTER) {
			tq = lt->qual | rt->qual;
			lt = lt->base;
			rt = rt->base;
			if (lt == &typevoid || rt == &typevoid)
				t = &typevoid;
			else if (typecompatible(lt, rt))
				t = typecomposite(lt, rt);
			else
				error(&tok.loc, "operands of conditional operator must have compatible types");
			t = mkpointertype(t, tq);
		} else {
			error(&tok.loc, "invalid operands to conditional operator");
		}
	}
	e = eval(e, EVALARITH);
	if (e->kind == EXPRCONST && e->type->prop & PROPINT)
		return exprconvert(e->constant.u ? l : r, t);
	e = mkexpr(EXPRCOND, t, e);
	e->cond.t = l;
	e->cond.f = r;
	return e;
}

struct expr *
constexpr(struct scope *s)
{
	return eval(condexpr(s), EVALARITH);
}

uint64_t
intconstexpr(struct scope *s, bool allowneg)
{
	struct expr *e;

	e = constexpr(s);
	if (e->kind != EXPRCONST || !(e->type->prop & PROPINT))
		error(&tok.loc, "not an integer constant expression");
	if (!allowneg && e->type->basic.issigned && e->constant.u > INT64_MAX)
		error(&tok.loc, "integer constant expression cannot be negative");
	return e->constant.u;
}

static struct expr *
mkassignexpr(struct expr *l, struct expr *r)
{
	struct expr *e;

	e = mkexpr(EXPRASSIGN, l->type, NULL);
	e->assign.l = l;
	e->assign.r = exprconvert(r, l->type);
	return e;
}

struct expr *
assignexpr(struct scope *s)
{
	struct expr *e, *l, *r, *tmp, *bit;
	enum tokenkind op;

	l = condexpr(s);
	if (l->kind == EXPRBINARY || l->kind == EXPRCOMMA || l->kind == EXPRCAST)
		return l;
	switch (tok.kind) {
	case TASSIGN:     op = TNONE; break;
	case TMULASSIGN:  op = TMUL;  break;
	case TDIVASSIGN:  op = TDIV;  break;
	case TMODASSIGN:  op = TMOD;  break;
	case TADDASSIGN:  op = TADD;  break;
	case TSUBASSIGN:  op = TSUB;  break;
	case TSHLASSIGN:  op = TSHL;  break;
	case TSHRASSIGN:  op = TSHR;  break;
	case TBANDASSIGN: op = TBAND; break;
	case TXORASSIGN:  op = TXOR;  break;
	case TBORASSIGN:  op = TBOR;  break;
	default:
		return l;
	}
	if (!l->lvalue)
		error(&tok.loc, "left side of assignment expression is not an lvalue");
	next();
	r = assignexpr(s);
	if (!op)
		return mkassignexpr(l, r);
	/* rewrite `E1 OP= E2` as `T = &E1, *T = *T OP E2`, where T is a temporary slot */
	if (l->kind == EXPRBITFIELD) {
		bit = l;
		l = l->base;
	} else {
		bit = NULL;
	}
	tmp = mkexpr(EXPRTEMP, mkpointertype(l->type, l->qual), NULL);
	tmp->lvalue = true;
	tmp->temp = NULL;
	e = mkassignexpr(tmp, mkunaryexpr(TBAND, l));
	l = mkunaryexpr(TMUL, tmp);
	if (bit) {
		bit->base = l;
		l = bit;
	}
	r = mkbinaryexpr(&tok.loc, op, l, r);
	e->next = mkassignexpr(l, r);
	return mkexpr(EXPRCOMMA, l->type, e);
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
	return mkexpr(EXPRCOMMA, e->type, r);
}

struct expr *
exprconvert(struct expr *e, struct type *t)
{
	if (typecompatible(e->type, t))
		return e;
	return mkexpr(EXPRCAST, t, e);
}
