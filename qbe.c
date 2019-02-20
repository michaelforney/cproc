#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "util.h"
#include "backend.h"
#include "decl.h"
#include "eval.h"
#include "expr.h"
#include "htab.h"
#include "init.h"
#include "scope.h"
#include "token.h"
#include "tree.h"
#include "type.h"

struct name {
	char *str;
	uint64_t id;
};

struct representation {
	char base;
	char ext;
	struct name abi;
};

struct value {
	enum {
		VALNONE,
		VALGLOBAL,
		VALCONST,
		VALLABEL,
		VALTEMP,
		VALELLIPSIS,
	} kind;
	struct representation *repr;
	union {
		struct name name;
		uint64_t i;
		double f;
	};
};

enum instructionkind {
	INONE,

#define OP(op, ret, arg, name) op,
#include "ops.h"
#undef OP
};

struct instruction {
	enum instructionkind kind;
	struct value res, *arg[];
};

struct block {
	struct value label;
	bool terminated;
	struct array insts;

	struct block *next;
};

struct switchcase {
	struct treenode node;
	struct value *body;
};

struct representation i8 = {'w', 'b'};
struct representation i16 = {'w', 'h'};
struct representation i32 = {'w', 'w'};
struct representation i64 = {'l', 'l'};
struct representation f32 = {'s', 's'};
struct representation f64 = {'d', 'd'};
struct representation iptr = {'l', 'l'};

void
switchcase(struct switchcases *cases, uint64_t i, struct value *v)
{
	struct switchcase *c;

	c = treeinsert(&cases->root, i, sizeof(*c));
	if (!c->node.new)
		error(&tok.loc, "multiple 'case' labels with same value");
	c->body = v;
}

/* functions */

struct value *
mkblock(char *name)
{
	static uint64_t id;
	struct block *b;

	b = xmalloc(sizeof(*b));
	b->label.kind = VALLABEL;
	b->label.name.str = name;
	b->label.name.id = ++id;
	b->terminated = false;
	b->insts = (struct array){0};
	b->next = NULL;

	return &b->label;
}

static void emittype(struct type *);
static void emitvalue(struct value *);

static void
funcname(struct function *f, struct name *n, char *s)
{
	n->id = ++f->lastid;
	n->str = s;
}

static void
functemp(struct function *f, struct value *v, struct representation *repr)
{
	if (!repr)
		fatal("temp has no type");
	v->kind = VALTEMP;
	funcname(f, &v->name, NULL);
	v->repr = repr;
}

static struct {
	int ret, arg;
	char *str;
} instdesc[] = {
#define OP(op, ret, arg, name) [op] = {ret, arg, name},
#include "ops.h"
#undef OP
};

static struct value *
funcinst(struct function *f, int op, struct representation *repr, struct value *args[])
{
	struct instruction *inst;
	size_t n;

	if (f->end->terminated)
		return NULL;
	n = instdesc[op].arg;
	if (n == -1) {
		for (n = 0; args[n]; ++n)
			;
		++n;
	}
	if (n > (SIZE_MAX - sizeof(*inst)) / sizeof(args[0])) {
		errno = ENOMEM;
		fatal("malloc:");
	}
	inst = xmalloc(sizeof(*inst) + n * sizeof(args[0]));
	inst->kind = op;
	if (repr)
		functemp(f, &inst->res, repr);
	else
		inst->res.kind = VALNONE;
	memcpy(inst->arg, args, n * sizeof(args[0]));
	arrayaddptr(&f->end->insts, inst);

	return instdesc[op].ret ? &inst->res : NULL;
}

struct value *
mkintconst(struct representation *r, uint64_t n)
{
	struct value *v;

	v = xmalloc(sizeof(*v));
	v->kind = VALCONST;
	v->repr = r;
	v->i = n;

	return v;
}

uint64_t
intconstvalue(struct value *v)
{
	assert(v->kind == VALCONST);
	return v->i;
}

static struct value *
mkfltconst(struct representation *r, double n)
{
	struct value *v;

	v = xmalloc(sizeof(*v));
	v->kind = VALCONST;
	v->repr = r;
	v->f = n;

	return v;
}

static void
funcalloc(struct function *f, struct declaration *d)
{
	enum instructionkind op;
	struct instruction *inst;

	assert(!d->type->incomplete);
	assert(d->type->size > 0);
	if (!d->align)
		d->align = d->type->align;
	else if (d->align < d->type->align)
		error(&tok.loc, "object requires alignment %d, which is stricter than %d", d->type->align, d->align);
	switch (d->align) {
	case 1:
	case 2:
	case 4:  op = IALLOC4; break;
	case 8:  op = IALLOC8; break;
	case 16: op = IALLOC16; break;
	default:
		fatal("internal error: invalid alignment: %d\n", d->align);
	}
	inst = xmalloc(sizeof(*inst) + sizeof(inst->arg[0]));
	inst->kind = op;
	functemp(f, &inst->res, &iptr);
	inst->arg[0] = mkintconst(&i64, d->type->size);
	d->value = &inst->res;
	arrayaddptr(&f->start->insts, inst);
}

static void
funcstore(struct function *f, struct type *t, struct value *addr, struct value *v)
{
	enum instructionkind op;
	enum typequalifier tq = QUALNONE;

	t = typeunqual(t, &tq);
	if (tq & QUALVOLATILE)
		error(&tok.loc, "volatile store is not yet supported");
	if (tq & QUALCONST)
		error(&tok.loc, "cannot store to 'const' object");
	switch (t->kind) {
	case TYPEBASIC:
		switch (t->size) {
		case 1: op = ISTOREB; break;
		case 2: op = ISTOREH; break;
		case 4: op = typeprop(t) & PROPFLOAT ? ISTORES : ISTOREW; break;
		case 8: op = typeprop(t) & PROPFLOAT ? ISTORED : ISTOREL; break;
		default:
			fatal("internal error; unimplemented store");
		}
		break;
	case TYPEPOINTER:
		op = ISTOREL;
		break;
	case TYPESTRUCT:
	case TYPEUNION:
	case TYPEARRAY: {
		enum instructionkind loadop, op;
		struct value *src, *dst, *tmp, *align;
		uint64_t offset;

		switch (t->align) {
		case 1: loadop = ILOADUB, op = ISTOREB; break;
		case 2: loadop = ILOADUH, op = ISTOREH; break;
		case 4: loadop = ILOADUW, op = ISTOREW; break;
		case 8: loadop = ILOADL, op = ISTOREL; break;
		default:
			fatal("internal error; invalid alignment %d", t->align);
		}
		src = v;
		dst = addr;
		align = mkintconst(&iptr, t->align);
		for (offset = 0; offset < t->size; offset += t->align) {
			tmp = funcinst(f, loadop, &iptr, (struct value *[]){src});
			funcinst(f, op, NULL, (struct value *[]){tmp, dst});
			src = funcinst(f, IADD, &iptr, (struct value *[]){src, align});
			dst = funcinst(f, IADD, &iptr, (struct value *[]){dst, align});
		}
		return;
	}
	default:
		fatal("unimplemented store");
	}
	funcinst(f, op, NULL, (struct value *[]){v, addr});
}

static struct value *
funcload(struct function *f, struct type *t, struct value *addr)
{
	struct value *v;
	enum instructionkind op;
	enum typequalifier tq;

	t = typeunqual(t, &tq);
	switch (t->kind) {
	case TYPEBASIC:
		switch (t->size) {
		case 1: op = t->basic.issigned ? ILOADSB : ILOADUB; break;
		case 2: op = t->basic.issigned ? ILOADSH : ILOADUH; break;
		case 4: op = typeprop(t) & PROPFLOAT ? ILOADS : t->basic.issigned ? ILOADSW : ILOADUW; break;
		case 8: op = typeprop(t) & PROPFLOAT ? ILOADD : ILOADL; break;
		default:
			fatal("internal error; unimplemented load");
		}
		break;
	case TYPEPOINTER:
		op = ILOADL;
		break;
	case TYPESTRUCT:
	case TYPEUNION:
	case TYPEARRAY:
		v = xmalloc(sizeof(*v));
		*v = *addr;
		v->repr = t->repr;
		return v;
	default:
		fatal("unimplemented load %d", t->kind);
	}
	return funcinst(f, op, t->repr, (struct value *[]){addr});
}

struct value *
mkglobal(char *name, bool private)
{
	static uint64_t id;
	struct value *v;

	v = xmalloc(sizeof(*v));
	v->kind = VALGLOBAL;
	v->repr = &iptr;
	v->name.str = name;
	v->name.id = private ? ++id : 0;

	return v;
}

/*
XXX: If a function declared without a prototype is declared with a
parameter affected by default argument promotion, we need to emit a QBE
function with the promoted type and implicitly convert to the declared
parameter type before storing into the allocated memory for the parameter.
*/
struct function *
mkfunc(char *name, struct type *t, struct scope *s)
{
	struct function *f;
	struct parameter *p;
	struct declaration *d;

	f = xmalloc(sizeof(*f));
	f->name = name;
	f->type = t;
	f->start = f->end = (struct block *)mkblock("start");
	f->gotos = mkhtab(8);
	f->lastid = 0;
	emittype(t->base);

	/* allocate space for parameters */
	for (p = t->func.params; p; p = p->next) {
		if (!p->name)
			error(&tok.loc, "parameter name omitted in function definition");
		if (!t->func.isprototype && !typecompatible(p->type, typeargpromote(p->type)))
			error(&tok.loc, "old-style function definition with parameter type incompatible with promoted type is not yet supported");
		emittype(p->type);
		d = mkdecl(DECLOBJECT, p->type, LINKNONE);
		p->value = xmalloc(sizeof(*p->value));
		functemp(f, p->value, p->type->repr);
		if (p->type->repr->abi.id) {
			d->value = xmalloc(sizeof(*d->value));
			*d->value = *p->value;
			d->value->repr = &iptr;
		} else {
			funcinit(f, d, NULL);
			funcstore(f, typeunqual(p->type, NULL), d->value, p->value);
		}
		scopeputdecl(s, p->name, d);
	}

	t = mkarraytype(mkqualifiedtype(&typechar, QUALCONST), strlen(name) + 1);
	d = mkdecl(DECLOBJECT, t, LINKNONE);
	d->value = mkglobal("__func__", true);
	scopeputdecl(s, "__func__", d);
	f->namedecl = d;

	funclabel(f, mkblock("body"));

	return f;
}

void
funclabel(struct function *f, struct value *v)
{
	assert(v->kind == VALLABEL);
	f->end->next = (struct block *)v;
	f->end = f->end->next;
}

void
funcjmp(struct function *f, struct value *v)
{
	funcinst(f, IJMP, NULL, (struct value *[]){v});
	f->end->terminated = true;
}

void
funcjnz(struct function *f, struct value *v, struct value *l1, struct value *l2)
{
	funcinst(f, IJNZ, NULL, (struct value *[]){v, l1, l2});
	f->end->terminated = true;
}

void
funcret(struct function *f, struct value *v)
{
	funcinst(f, IRET, NULL, (struct value *[]){v});
	f->end->terminated = true;
}

struct gotolabel *
funcgoto(struct function *f, char *name)
{
	void **entry;
	struct gotolabel *g;
	struct hashtablekey key;

	htabstrkey(&key, name);
	entry = htabput(f->gotos, &key);
	g = *entry;
	if (!g) {
		g = xmalloc(sizeof(*g));
		g->label = mkblock(name);
		*entry = g;
	}

	return g;
}

static struct value *
objectaddr(struct function *f, struct expression *e)
{
	struct declaration *d;

	switch (e->kind) {
	case EXPRIDENT:
		d = e->ident.decl;
		if (d->kind != DECLOBJECT && d->kind != DECLFUNC)
			error(&tok.loc, "identifier is not an object or function");  // XXX: fix location, var name
		if (d == f->namedecl) {
			fputs("data ", stdout);
			emitvalue(d->value);
			printf(" = { b \"%s\", b 0 }\n", f->name);
			f->namedecl = NULL;
		}
		return d->value;
	case EXPRSTRING:
		d = stringdecl(e);
		return d->value;
	case EXPRCOMPOUND:
		d = mkdecl(DECLOBJECT, e->type, LINKNONE);
		funcinit(f, d, e->compound.init);
		return d->value;
	case EXPRUNARY:
		if (e->unary.op != TMUL)
			break;
		return funcexpr(f, e->unary.base);
	default:
		if (e->type->kind != TYPESTRUCT && e->type->kind != TYPEUNION)
			break;
		return funcinst(f, ICOPY, &iptr, (struct value *[]){funcexpr(f, e)});
	}
	error(&tok.loc, "expression is not an object");
}

/* TODO: move these conversions to QBE */
static struct value *
utof(struct function *f, struct representation *r, struct value *v)
{
	struct value *odd, *big, *phi[5] = {0}, *join;

	if (v->repr->base == 'w') {
		v = funcinst(f, IEXTUW, &i64, (struct value *[]){v});
		return funcinst(f, ISLTOF, r, (struct value *[]){v});
	}

	phi[0] = mkblock("utof_small");
	phi[2] = mkblock("utof_big");
	join = mkblock("utof_join");

	big = funcinst(f, ICSLTL, &i32, (struct value *[]){v, mkintconst(&i64, 0)});
	funcjnz(f, big, phi[2], phi[0]);

	funclabel(f, phi[0]);
	phi[1] = funcinst(f, ISLTOF, r, (struct value *[]){v});
	funcjmp(f, join);

	funclabel(f, phi[2]);
	odd = funcinst(f, IAND, &i64, (struct value *[]){v, mkintconst(&i64, 1)});
	v = funcinst(f, ISHR, &i64, (struct value *[]){v, mkintconst(&i64, 1)});
	v = funcinst(f, IOR, &i64, (struct value *[]){v, odd});  /* round to odd */
	v = funcinst(f, ISLTOF, r, (struct value *[]){v});
	phi[3] = funcinst(f, IADD, r, (struct value *[]){v, v});

	funclabel(f, join);
	return funcinst(f, IPHI, r, phi);
}

static struct value *
ftou(struct function *f, struct representation *r, struct value *v)
{
	struct value *big, *phi[5] = {0}, *join, *maxflt, *maxint;
	enum instructionkind op = v->repr->base == 's' ? ISTOSI : IDTOSI;

	if (r->base == 'w') {
		v = funcinst(f, op, &i64, (struct value *[]){v});
		return funcinst(f, ICOPY, r, (struct value *[]){v});
	}

	phi[0] = mkblock("ftou_small");
	phi[2] = mkblock("ftou_big");
	join = mkblock("ftou_join");

	maxflt = mkfltconst(v->repr, 0x1p63);
	maxint = mkintconst(&i64, 1ull<<63);

	big = funcinst(f, v->repr->base == 's' ? ICGES : ICGED, &i32, (struct value *[]){v, maxflt});
	funcjnz(f, big, phi[2], phi[0]);

	funclabel(f, phi[0]);
	phi[1] = funcinst(f, op, r, (struct value *[]){v});
	funcjmp(f, join);

	funclabel(f, phi[2]);
	v = funcinst(f, ISUB, v->repr, (struct value *[]){v, maxflt});
	v = funcinst(f, op, r, (struct value *[]){v});
	phi[3] = funcinst(f, IXOR, r, (struct value *[]){v, maxint});

	funclabel(f, join);
	return funcinst(f, IPHI, r, phi);
}

static struct value *
extend(struct function *f, struct type *t, struct value *v)
{
	enum instructionkind op;

	switch (t->size) {
	case 1: op = t->basic.issigned ? IEXTSB : IEXTUB; break;
	case 2: op = t->basic.issigned ? IEXTSH : IEXTUH; break;
	default: return v;
	}
	return funcinst(f, op, &i32, (struct value *[]){v});
}

struct value *
funcexpr(struct function *f, struct expression *e)
{
	static struct value ellipsis = {.kind = VALELLIPSIS};
	enum instructionkind op = INONE;
	struct declaration *d;
	struct value *l, *r, *v, *addr, **argvals, **argval;
	struct expression *arg;
	struct value *label[5];
	struct type *t;

	switch (e->kind) {
	case EXPRIDENT:
		d = e->ident.decl;
		switch (d->kind) {
		case DECLOBJECT: return funcload(f, d->type, d->value);
		case DECLCONST:  return d->value;
		default:
			fatal("unimplemented declaration kind %d", d->kind);
		}
		break;
	case EXPRCONST:
		if (typeprop(e->type) & PROPINT || e->type->kind == TYPEPOINTER)
			return mkintconst(e->type->repr, e->constant.i);
		return mkfltconst(e->type->repr, e->constant.f);
	case EXPRCOMPOUND:
		l = objectaddr(f, e);
		return funcload(f, e->type, l);
	case EXPRINCDEC:
		addr = objectaddr(f, e->incdec.base);
		l = funcload(f, e->incdec.base->type, addr);
		if (e->type->kind == TYPEPOINTER)
			r = mkintconst(e->type->repr, e->type->base->size);
		else if (typeprop(e->type) & PROPINT)
			r = mkintconst(e->type->repr, 1);
		else if (typeprop(e->type) & PROPFLOAT)
			r = mkfltconst(e->type->repr, 1);
		else
			fatal("not a scalar");
		v = funcinst(f, e->incdec.op == TINC ? IADD : ISUB, e->type->repr, (struct value *[]){l, r});
		funcstore(f, e->type, addr, v);
		return e->incdec.post ? l : v;
	case EXPRCALL:
		argvals = xreallocarray(NULL, e->call.nargs + 3, sizeof(argvals[0]));
		argvals[0] = funcexpr(f, e->call.func);
		emittype(e->type);
		for (argval = &argvals[1], arg = e->call.args; arg; ++argval, arg = arg->next) {
			emittype(arg->type);
			*argval = funcexpr(f, arg);
		}
		if (e->call.func->type->base->func.isvararg)
			*argval++ = &ellipsis;
		*argval = NULL;
		return funcinst(f, ICALL, e->type == &typevoid ? NULL : e->type->repr, argvals);
		//if (e->call.func->type->base->func.isnoreturn)
		//	funcret(f, NULL);
	case EXPRUNARY:
		switch (e->unary.op) {
		case TBAND:
			return objectaddr(f, e->unary.base);
		case TMUL:
			r = funcexpr(f, e->unary.base);
			return funcload(f, e->type, r);
		}
		fatal("internal error; unknown unary expression");
		break;
	case EXPRCAST: {
		struct type *src, *dst;
		int srcprop, dstprop;

		l = funcexpr(f, e->cast.e);
		r = NULL;

		src = typeunqual(e->cast.e->type, NULL);
		if (src->kind == TYPEPOINTER)
			src = &typeulong;
		dst = typeunqual(e->type, NULL);
		if (dst->kind == TYPEPOINTER)
			dst = &typeulong;
		if (dst->kind == TYPEVOID)
			return NULL;
		if (src->kind != TYPEBASIC || dst->kind != TYPEBASIC)
			fatal("internal error; unsupported conversion");
		srcprop = typeprop(src);
		dstprop = typeprop(dst);
		if (dst->basic.kind == BASICBOOL) {
			l = extend(f, src, l);
			r = mkintconst(src->repr, 0);
			if (srcprop & PROPINT)
				op = src->size == 8 ? ICNEL : ICNEW;
			else
				op = src->size == 8 ? ICNED : ICNES;
		} else if (dstprop & PROPINT) {
			if (srcprop & PROPINT) {
				if (dst->size <= src->size) {
					op = ICOPY;
				} else {
					switch (src->size) {
					case 4: op = src->basic.issigned ? IEXTSW : IEXTUW; break;
					case 2: op = src->basic.issigned ? IEXTSH : IEXTUH; break;
					case 1: op = src->basic.issigned ? IEXTSB : IEXTUB; break;
					default: fatal("internal error; unknown int conversion");
					}
				}
			} else {
				if (!dst->basic.issigned)
					return ftou(f, dst->repr, l);
				op = src->size == 8 ? IDTOSI : ISTOSI;
			}
		} else {
			if (srcprop & PROPINT) {
				if (!src->basic.issigned)
					return utof(f, dst->repr, l);
				op = src->size == 8 ? ISLTOF : ISWTOF;
			} else {
				if (src->size < dst->size)
					op = IEXTS;
				else if (src->size > dst->size)
					op = ITRUNCD;
				else
					op = ICOPY;
			}
		}
		return funcinst(f, op, dst->repr, (struct value *[]){l, r});
	}
	case EXPRBINARY:
		if (e->binary.op == TLOR || e->binary.op == TLAND) {
			label[0] = mkblock("logic_right");
			label[1] = mkblock("logic_join");

			l = funcexpr(f, e->binary.l);
			label[2] = (struct value *)f->end;
			if (e->binary.op == TLOR)
				funcjnz(f, l, label[1], label[0]);
			else
				funcjnz(f, l, label[0], label[1]);
			funclabel(f, label[0]);
			r = funcexpr(f, e->binary.r);
			label[3] = (struct value *)f->end;
			funclabel(f, label[1]);
			return funcinst(f, IPHI, e->type->repr, (struct value *[]){label[2], l, label[3], r, NULL});
		}
		l = funcexpr(f, e->binary.l);
		r = funcexpr(f, e->binary.r);
		t = e->binary.l->type;
		if (t->kind == TYPEPOINTER)
			t = &typeulong;
		switch (e->binary.op) {
		case TMUL:
			op = IMUL;
			break;
		case TDIV:
			op = !(typeprop(e->type) & PROPINT) || e->type->basic.issigned ? IDIV : IUDIV;
			break;
		case TMOD:
			op = e->type->basic.issigned ? IREM : IUREM;
			break;
		case TADD:
			op = IADD;
			break;
		case TSUB:
			op = ISUB;
			break;
		case TSHL:
			op = ISHL;
			break;
		case TSHR:
			op = t->basic.issigned ? ISAR : ISHR;
			break;
		case TBOR:
			op = IOR;
			break;
		case TBAND:
			op = IAND;
			break;
		case TXOR:
			op = IXOR;
			break;
		case TLESS:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = typeprop(t) & PROPFLOAT ? ICLTS : t->basic.issigned ? ICSLTW : ICULTW;
			else
				op = typeprop(t) & PROPFLOAT ? ICLTD : t->basic.issigned ? ICSLTL : ICULTL;
			break;
		case TGREATER:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = typeprop(t) & PROPFLOAT ? ICGTS : t->basic.issigned ? ICSGTW : ICUGTW;
			else
				op = typeprop(t) & PROPFLOAT ? ICGTD : t->basic.issigned ? ICSGTL : ICUGTL;
			break;
		case TLEQ:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = typeprop(t) & PROPFLOAT ? ICLES : t->basic.issigned ? ICSLEW : ICULEW;
			else
				op = typeprop(t) & PROPFLOAT ? ICLED : t->basic.issigned ? ICSLEL : ICULEL;
			break;
		case TGEQ:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = typeprop(t) & PROPFLOAT ? ICGES : t->basic.issigned ? ICSGEW : ICUGEW;
			else
				op = typeprop(t) & PROPFLOAT ? ICGED : t->basic.issigned ? ICSGEL : ICUGEL;
			break;
		case TEQL:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = typeprop(t) & PROPFLOAT ? ICEQS : ICEQW;
			else
				op = typeprop(t) & PROPFLOAT ? ICEQD : ICEQL;
			break;
		case TNEQ:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = typeprop(t) & PROPFLOAT ? ICNES : ICNEW;
			else
				op = typeprop(t) & PROPFLOAT ? ICNED : ICNEL;
			break;
		}
		if (op == INONE)
			fatal("internal error; unimplemented binary expression");
		return funcinst(f, op, e->type->repr, (struct value *[]){l, r});
	case EXPRCOND:
		label[0] = mkblock("cond_true");
		label[1] = mkblock("cond_false");
		label[2] = mkblock("cond_join");

		v = funcexpr(f, e->cond.e);
		funcjnz(f, v, label[0], label[1]);

		funclabel(f, label[0]);
		l = funcexpr(f, e->cond.t);
		label[3] = (struct value *)f->end;
		funcjmp(f, label[2]);

		funclabel(f, label[1]);
		r = funcexpr(f, e->cond.f);
		label[4] = (struct value *)f->end;

		funclabel(f, label[2]);
		if (e->type == &typevoid)
			return NULL;
		return funcinst(f, IPHI, e->type->repr, (struct value *[]){label[3], l, label[4], r, NULL});
	case EXPRASSIGN:
		r = funcexpr(f, e->assign.r);
		if (e->assign.l->kind == EXPRTEMP) {
			e->assign.l->temp = r;
		} else {
			l = objectaddr(f, e->assign.l);
			funcstore(f, e->assign.l->type, l, r);
		}
		return r;
	case EXPRCOMMA:
		for (e = e->comma.exprs; e->next; e = e->next)
			funcexpr(f, e);
		return funcexpr(f, e);
	case EXPRBUILTIN:
		switch (e->builtin.kind) {
		case BUILTINVASTART:
			l = funcexpr(f, e->builtin.arg);
			funcinst(f, IVASTART, NULL, (struct value *[]){l});
			break;
		case BUILTINVAARG:
			l = funcexpr(f, e->builtin.arg);
			return funcinst(f, IVAARG, e->type->repr, (struct value *[]){l});
		case BUILTINVAEND:
			/* no-op */
			break;
		default:
			fatal("internal error: unimplemented builtin");
		}
		return NULL;
	case EXPRTEMP:
		assert(e->temp);
		return e->temp;
	default:
		fatal("unimplemented expression %d", e->kind);
	}
}

static void
zero(struct function *func, struct value *addr, int align, uint64_t offset, uint64_t end)
{
	enum instructionkind store[] = {
		[1] = ISTOREB,
		[2] = ISTOREH,
		[4] = ISTOREW,
		[8] = ISTOREL,
	};
	struct value *tmp;
	static struct value z = {.kind = VALCONST, .repr = &i64};
	int a = 1;

	while (offset < end) {
		if ((align - (offset & align - 1)) & a) {
			tmp = funcinst(func, IADD, &iptr, (struct value *[]){addr, mkintconst(&iptr, offset)});
			funcinst(func, store[a], NULL, (struct value *[]){&z, tmp});
			offset += a;
		}
		if (a < align)
			a <<= 1;
	}
}

void
funcinit(struct function *func, struct declaration *d, struct initializer *init)
{
	struct value *src, *dst;
	uint64_t offset = 0;
	size_t i;

	funcalloc(func, d);
	if (!init)
		return;
	for (; init; init = init->next) {
		zero(func, d->value, d->type->align, offset, init->start);
		if (init->expr->kind == EXPRSTRING) {
			for (i = 0; i <= init->expr->string.size && i < init->end - init->start; ++i) {
				dst = funcinst(func, IADD, &iptr, (struct value *[]){d->value, mkintconst(&iptr, init->start + i)});
				funcinst(func, ISTOREB, NULL, (struct value *[]){mkintconst(&i8, init->expr->string.data[i]), dst});
			}
		} else {
			dst = funcinst(func, IADD, &iptr, (struct value *[]){d->value, mkintconst(&iptr, init->start)});
			src = funcexpr(func, init->expr);
			funcstore(func, init->expr->type, dst, src);
		}
		offset = init->end;
	}
	zero(func, d->value, d->type->align, offset, d->type->size);
}

static void
casesearch(struct function *f, struct value *v, struct switchcase *c, struct value *defaultlabel)
{
	struct value *res, *label[3], *key;

	if (!c) {
		funcjmp(f, defaultlabel);
		return;
	}
	label[0] = mkblock("switch_ne");
	label[1] = mkblock("switch_lt");
	label[2] = mkblock("switch_gt");

	// XXX: linear search if c->node.height < 4
	key = mkintconst(&i64, c->node.key);
	res = funcinst(f, ICEQW, &i32, (struct value *[]){v, key});
	funcjnz(f, res, c->body, label[0]);
	funclabel(f, label[0]);
	res = funcinst(f, ICULTW, typeint.repr, (struct value *[]){v, key});
	funcjnz(f, res, label[1], label[2]);
	funclabel(f, label[1]);
	casesearch(f, v, c->node.child[0], defaultlabel);
	funclabel(f, label[2]);
	casesearch(f, v, c->node.child[1], defaultlabel);
}

void
funcswitch(struct function *f, struct value *v, struct switchcases *c, struct value *defaultlabel)
{
	casesearch(f, v, c->root, defaultlabel);
}

/* emit */

static void
emitname(struct name *n)
{
	if (n->str)
		fputs(n->str, stdout);
	if (n->id)
		printf(".%" PRIu64, n->id);
}

static void
emitvalue(struct value *v)
{
	static char sigil[] = {
		[VALLABEL] = '@',
		[VALTEMP] = '%',
		[VALGLOBAL] = '$',
	};

	switch (v->kind) {
	case VALCONST:
		if (v->repr->base == 's' || v->repr->base == 'd')
			printf("%c_%a", v->repr->base, v->f);
		else
			printf("%" PRIu64, v->i);
		break;
	case VALELLIPSIS:
		fputs("...", stdout);
		break;
	default:
		if (v->kind >= LEN(sigil) || !sigil[v->kind])
			fatal("invalid value");
		putchar(sigil[v->kind]);
		if (v->kind == VALGLOBAL && v->name.id)
			fputs(".L", stdout);
		emitname(&v->name);
	}
}

static void
emitrepr(struct representation *r, bool abi, bool ext)
{
	if (abi && r->abi.id) {
		putchar(':');
		emitname(&r->abi);
	} else if (ext) {
		putchar(r->ext);
	} else {
		putchar(r->base);
	}
}

/* XXX: need to consider _Alignas on struct members */
/* XXX: this might not be right for something like
union {
	struct { long long x; double y; };
	struct { double z; long long w; };
};
*/
static void
emittype(struct type *t)
{
	static uint64_t id;
	struct member *m;
	struct type *sub;
	uint64_t i;

	if (!t->repr || t->repr->abi.id || t->kind != TYPESTRUCT && t->kind != TYPEUNION)
		return;
	t->repr = xmalloc(sizeof(*t->repr));
	t->repr->base = 'l';
	t->repr->abi.str = t->structunion.tag;
	t->repr->abi.id = ++id;
	for (m = t->structunion.members; m; m = m->next) {
		for (sub = m->type; sub->kind == TYPEARRAY; sub = sub->base)
			;
		emittype(sub);
	}
	fputs("type :", stdout);
	emitname(&t->repr->abi);
	fputs(" = { ", stdout);
	for (m = t->structunion.members; m && t->kind == TYPESTRUCT; m = m->next) {
		for (i = 1, sub = m->type; sub->kind == TYPEARRAY; sub = sub->base)
			i *= sub->array.length;
		emitrepr(sub->repr, true, true);
		if (i > 1)
			printf(" %" PRIu64, i);
		fputs(", ", stdout);
	}
	puts("}");
}

static void
emitinst(struct instruction *inst)
{
	struct value **arg;

	putchar('\t');
	assert(inst->kind < LEN(instdesc));
	if (instdesc[inst->kind].ret && inst->res.kind) {
		emitvalue(&inst->res);
		fputs(" =", stdout);
		emitrepr(inst->res.repr, inst->kind == ICALL, false);
		putchar(' ');
	}
	fputs(instdesc[inst->kind].str, stdout);
	switch (inst->kind) {
	case ICALL:
		putchar(' ');
		emitvalue(inst->arg[0]);
		putchar('(');
		for (arg = &inst->arg[1]; *arg; ++arg) {
			if (arg != &inst->arg[1])
				fputs(", ", stdout);
			if ((*arg)->kind != VALELLIPSIS) {
				emitrepr((*arg)->repr, true, false);
				putchar(' ');
			}
			emitvalue(*arg);
		}
		putchar(')');
		break;
	case IPHI:
		putchar(' ');
		for (arg = inst->arg; *arg; ++arg) {
			if (arg != inst->arg)
				fputs(", ", stdout);
			emitvalue(*arg);
			putchar(' ');
			emitvalue(*++arg);
		}
		break;
	case IRET:
		if (!inst->arg[0])
			break;
		/* fallthrough */
	default:
		putchar(' ');
		emitvalue(inst->arg[0]);
		if (instdesc[inst->kind].arg > 1) {
			fputs(", ", stdout);
			emitvalue(inst->arg[1]);
		}
		if (instdesc[inst->kind].arg > 2) {
			fputs(", ", stdout);
			emitvalue(inst->arg[2]);
		}
	}
	putchar('\n');
}

void
emitfunc(struct function *f, bool global)
{
	struct block *b;
	struct instruction **inst;
	struct parameter *p;
	size_t n;

	if (!f->end->terminated)
		funcret(f, strcmp(f->name, "main") == 0 ? mkintconst(&i32, 0) : NULL);
	if (global)
		puts("export");
	fputs("function ", stdout);
	if (f->type->base != &typevoid) {
		emitrepr(f->type->base->repr, true, false);
		putchar(' ');
	}
	printf("$%s(", f->name);
	for (p = f->type->func.params; p; p = p->next) {
		if (p != f->type->func.params)
			fputs(", ", stdout);
		emitrepr(p->type->repr, true, false);
		putchar(' ');
		emitvalue(p->value);
	}
	if (f->type->func.isvararg)
		fputs(", ...", stdout);
	puts(") {");
	for (b = f->start; b; b = b->next) {
		emitvalue(&b->label);
		putchar('\n');
		n = b->insts.len / sizeof(*inst);
		for (inst = b->insts.val; n; --n, ++inst)
			emitinst(*inst);
	}
	puts("}");
}

static void
dataitem(struct expression *expr, uint64_t size)
{
	struct declaration *decl;
	size_t i;
	char c;

	switch (expr->kind) {
	case EXPRUNARY:
		if (expr->unary.op != TBAND)
			fatal("not a address expr");
		expr = expr->unary.base;
		if (expr->kind != EXPRIDENT)
			error(&tok.loc, "initializer is not a constant expression");
		decl = expr->ident.decl;
		if (decl->value->kind != VALGLOBAL)
			fatal("not a global");
		emitvalue(decl->value);
		break;
	case EXPRBINARY:
		if (expr->binary.l->kind != EXPRUNARY || expr->binary.r->kind != EXPRCONST)
			error(&tok.loc, "initializer is not a constant expression");
		dataitem(expr->binary.l, 0);
		fputs(" + ", stdout);
		dataitem(expr->binary.r, 0);
		break;
	case EXPRCONST:
		if (typeprop(expr->type) & PROPFLOAT)
			printf("%c_%a", expr->type->size == 4 ? 's' : 'd', expr->constant.f);
		else
			printf("%" PRIu64, expr->constant.i);
		break;
	case EXPRSTRING:
		fputc('"', stdout);
		for (i = 0; i < expr->string.size && i < size; ++i) {
			c = expr->string.data[i];
			if (isprint(c) && c != '"' && c != '\\')
				putchar(c);
			else
				printf("\\%03hho", c);
		}
		fputc('"', stdout);
		if (i < size)
			printf(", z %" PRIu64, size - i);
		break;
	default:
		fatal("unimplemented initdata");
	}
}

void
emitdata(struct declaration *d, struct initializer *init)
{
	uint64_t offset = 0;
	struct initializer *cur;

	if (!d->align)
		d->align = d->type->align;
	else if (d->align < d->type->align)
		error(&tok.loc, "object requires alignment %d, which is stricter than %d", d->type->align, d->align);
	for (cur = init; cur; cur = cur->next)
		cur->expr = eval(cur->expr);
	if (d->linkage == LINKEXTERN)
		fputs("export ", stdout);
	fputs("data ", stdout);
	emitvalue(d->value);
	printf(" = align %d { ", d->align);

	for (; init; offset = init->end, init = init->next) {
		if (offset < init->start)
			printf("z %" PRIu64 ", ", init->start - offset);
		printf("%c ", init->expr->type->kind == TYPEARRAY ? init->expr->type->base->repr->ext : init->expr->type->repr->ext);
		dataitem(init->expr, init->end - init->start);
		fputs(", ", stdout);
	}
	assert(offset <= d->type->size);
	if (offset < d->type->size)
		printf("z %" PRIu64 " ", d->type->size - offset);
	puts("}");
}
