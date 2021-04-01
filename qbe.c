#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "util.h"
#include "cc.h"

struct name {
	char *str;
	uint64_t id;
};

struct repr {
	char base;
	char ext;
};

struct value {
	enum {
		VALUE_NONE,
		VALUE_GLOBAL,
		VALUE_CONST,
		VALUE_TEMP,
		VALUE_TYPE,
	} kind;
	struct repr *repr;
	union {
		struct name name;
		uint64_t i;
		double f;
	};
};

struct lvalue {
	struct value *addr;
	struct bitfield bits;
};

enum instkind {
	INONE,

#define OP(op, name) op,
#include "ops.h"
#undef OP

	IARG,
};

struct inst {
	enum instkind kind;
	struct value res, *arg[2];
};

struct jump {
	enum {
		JUMP_NONE,
		JUMP_JMP,
		JUMP_JNZ,
		JUMP_RET,
	} kind;
	struct value *arg;
	struct block *blk[2];
};

struct block {
	struct name label;
	struct array insts;
	struct {
		struct block *blk[2];
		struct value *val[2];
		struct value res;
	} phi;
	struct jump jump;

	struct block *next;
};

struct switchcase {
	struct treenode node;
	struct block *body;
};

struct func {
	struct decl *decl, *namedecl;
	char *name;
	struct type *type;
	struct block *start, *end;
	struct map *gotos;
	uint64_t lastid;
};

struct repr i8 = {'w', 'b'};
struct repr i16 = {'w', 'h'};
struct repr i32 = {'w', 'w'};
struct repr i64 = {'l', 'l'};
struct repr f32 = {'s', 's'};
struct repr f64 = {'d', 'd'};
struct repr iptr = {'l', 'l'};

void
switchcase(struct switchcases *cases, uint64_t i, struct block *b)
{
	struct switchcase *c;

	c = treeinsert(&cases->root, i, sizeof(*c));
	if (!c->node.new)
		error(&tok.loc, "multiple 'case' labels with same value");
	c->body = b;
}

/* values */

struct block *
mkblock(char *name)
{
	static uint64_t id;
	struct block *b;

	b = xmalloc(sizeof(*b));
	b->label.str = name;
	b->label.id = ++id;
	b->insts = (struct array){0};
	b->jump.kind = JUMP_NONE;
	b->phi.res.kind = VALUE_NONE;
	b->next = NULL;

	return b;
}

struct value *
mkglobal(char *name, bool private)
{
	static uint64_t id;
	struct value *v;

	v = xmalloc(sizeof(*v));
	v->kind = VALUE_GLOBAL;
	v->repr = &iptr;
	v->name.str = name;
	v->name.id = private ? ++id : 0;

	return v;
}

char *
globalname(struct value *v)
{
	assert(v->kind == VALUE_GLOBAL && !v->name.id);
	return v->name.str;
}

struct value *
mkintconst(struct repr *r, uint64_t n)
{
	struct value *v;

	v = xmalloc(sizeof(*v));
	v->kind = VALUE_CONST;
	v->repr = r;
	v->i = n;

	return v;
}

uint64_t
intconstvalue(struct value *v)
{
	assert(v->kind == VALUE_CONST);
	return v->i;
}

static struct value *
mkfltconst(struct repr *r, double n)
{
	struct value *v;

	v = xmalloc(sizeof(*v));
	v->kind = VALUE_CONST;
	v->repr = r;
	v->f = n;

	return v;
}

/* functions */

static void emittype(struct type *);
static void emitvalue(struct value *);

static void
funcname(struct func *f, struct name *n, char *s)
{
	n->id = ++f->lastid;
	n->str = s;
}

static void
functemp(struct func *f, struct value *v, struct repr *repr)
{
	if (!repr)
		fatal("temp has no type");
	v->kind = VALUE_TEMP;
	funcname(f, &v->name, NULL);
	v->repr = repr;
}

static const char *instname[] = {
#define OP(op, name) [op] = name,
#include "ops.h"
#undef OP
};

static struct value *
funcinst(struct func *f, int op, struct repr *repr, struct value *arg0, struct value *arg1)
{
	struct inst *inst;

	if (f->end->jump.kind)
		return NULL;
	inst = xmalloc(sizeof(*inst));
	inst->kind = op;
	inst->arg[0] = arg0;
	inst->arg[1] = arg1;
	if (repr)
		functemp(f, &inst->res, repr);
	else
		inst->res.kind = VALUE_NONE;
	arrayaddptr(&f->end->insts, inst);

	return &inst->res;
}

static void
funcalloc(struct func *f, struct decl *d)
{
	enum instkind op;
	struct inst *inst;

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
	inst = xmalloc(sizeof(*inst));
	inst->kind = op;
	functemp(f, &inst->res, &iptr);
	inst->arg[0] = mkintconst(&i64, d->type->size);
	inst->arg[1] = NULL;
	d->value = &inst->res;
	arrayaddptr(&f->start->insts, inst);
}

static struct value *
funcbits(struct func *f, struct type *t, struct value *v, struct bitfield b)
{
	if (b.after)
		v = funcinst(f, ISHL, t->repr, v, mkintconst(&i32, b.after));
	if (b.before + b.after)
		v = funcinst(f, t->basic.issigned ? ISAR : ISHR, t->repr, v, mkintconst(&i32, b.before + b.after));
	return v;
}

static struct value *
funcstore(struct func *f, struct type *t, enum typequal tq, struct lvalue lval, struct value *v)
{
	struct value *r;
	enum instkind loadop, storeop;
	enum typeprop tp;
	unsigned long long mask;

	if (tq & QUALVOLATILE)
		error(&tok.loc, "volatile store is not yet supported");
	if (tq & QUALCONST)
		error(&tok.loc, "cannot store to 'const' object");
	tp = t->prop;
	assert(!lval.bits.before && !lval.bits.after || tp & PROPINT);
	r = v;
	switch (t->kind) {
	case TYPESTRUCT:
	case TYPEUNION:
	case TYPEARRAY: {
		struct value *src, *dst, *tmp, *align;
		uint64_t offset;

		switch (t->align) {
		case 1: loadop = ILOADUB, storeop = ISTOREB; break;
		case 2: loadop = ILOADUH, storeop = ISTOREH; break;
		case 4: loadop = ILOADUW, storeop = ISTOREW; break;
		case 8: loadop = ILOADL, storeop = ISTOREL; break;
		default:
			fatal("internal error; invalid alignment %d", t->align);
		}
		src = v;
		dst = lval.addr;
		align = mkintconst(&iptr, t->align);
		for (offset = 0; offset < t->size; offset += t->align) {
			tmp = funcinst(f, loadop, &iptr, src, NULL);
			funcinst(f, storeop, NULL, tmp, dst);
			src = funcinst(f, IADD, &iptr, src, align);
			dst = funcinst(f, IADD, &iptr, dst, align);
		}
		break;
	}
	case TYPEPOINTER:
		t = &typeulong;
		/* fallthrough */
	default:
		assert(tp & PROPSCALAR);
		switch (t->size) {
		case 1: loadop = ILOADUB; storeop = ISTOREB; break;
		case 2: loadop = ILOADUH; storeop = ISTOREH; break;
		case 4: loadop = ILOADUW; storeop = tp & PROPFLOAT ? ISTORES : ISTOREW; break;
		case 8: loadop = ILOADL; storeop = tp & PROPFLOAT ? ISTORED : ISTOREL; break;
		default:
			fatal("internal error; unimplemented store");
		}
		if (lval.bits.before || lval.bits.after) {
			mask = 0xffffffffffffffffu >> lval.bits.after + 64 - t->size * 8 ^ (1 << lval.bits.before) - 1;
			v = funcinst(f, ISHL, t->repr, v, mkintconst(&i32, lval.bits.before));
			r = funcbits(f, t, v, lval.bits);
			v = funcinst(f, IAND, t->repr, v, mkintconst(t->repr, mask));
			v = funcinst(f, IOR, t->repr, v,
				funcinst(f, IAND, t->repr,
					funcinst(f, loadop, t->repr, lval.addr, NULL),
					mkintconst(t->repr, ~mask)
				)
			);
		}
		funcinst(f, storeop, NULL, v, lval.addr);
		break;
	}
	return r;
}

static struct value *
funcload(struct func *f, struct type *t, struct lvalue lval)
{
	struct value *v;
	enum instkind op;

	switch (t->kind) {
	case TYPEPOINTER:
		op = ILOADL;
		break;
	case TYPESTRUCT:
	case TYPEUNION:
	case TYPEARRAY:
		v = xmalloc(sizeof(*v));
		*v = *lval.addr;
		v->repr = t->repr;
		return v;
	default:
		assert(t->prop & PROPREAL);
		switch (t->size) {
		case 1: op = t->basic.issigned ? ILOADSB : ILOADUB; break;
		case 2: op = t->basic.issigned ? ILOADSH : ILOADUH; break;
		case 4: op = t->prop & PROPFLOAT ? ILOADS : t->basic.issigned ? ILOADSW : ILOADUW; break;
		case 8: op = t->prop & PROPFLOAT ? ILOADD : ILOADL; break;
		default:
			fatal("internal error; unimplemented load");
		}
	}
	v = funcinst(f, op, t->repr, lval.addr, NULL);
	return funcbits(f, t, v, lval.bits);
}

/* TODO: move these conversions to QBE */
static struct value *
utof(struct func *f, struct repr *r, struct value *v)
{
	struct value *odd, *big;
	struct block *join;

	if (v->repr->base == 'w') {
		v = funcinst(f, IEXTUW, &i64, v, NULL);
		return funcinst(f, ISLTOF, r, v, NULL);
	}

	join = mkblock("utof_join");
	join->phi.blk[0] = mkblock("utof_small");
	join->phi.blk[1] = mkblock("utof_big");

	big = funcinst(f, ICSLTL, &i32, v, mkintconst(&i64, 0));
	funcjnz(f, big, join->phi.blk[1], join->phi.blk[0]);

	funclabel(f, join->phi.blk[0]);
	join->phi.val[0] = funcinst(f, ISLTOF, r, v, NULL);
	funcjmp(f, join);

	funclabel(f, join->phi.blk[1]);
	odd = funcinst(f, IAND, &i64, v, mkintconst(&i64, 1));
	v = funcinst(f, ISHR, &i64, v, mkintconst(&i64, 1));
	v = funcinst(f, IOR, &i64, v, odd);  /* round to odd */
	v = funcinst(f, ISLTOF, r, v, NULL);
	join->phi.val[1] = funcinst(f, IADD, r, v, v);

	funclabel(f, join);
	functemp(f, &join->phi.res, r);
	return &join->phi.res;
}

static struct value *
ftou(struct func *f, struct repr *r, struct value *v)
{
	struct value *big, *maxflt, *maxint;
	struct block *join;
	enum instkind op = v->repr->base == 's' ? ISTOSI : IDTOSI;

	if (r->base == 'w') {
		v = funcinst(f, op, &i64, v, NULL);
		return funcinst(f, ICOPY, r, v, NULL);
	}

	join = mkblock("ftou_join");
	join->phi.blk[0] = mkblock("ftou_small");
	join->phi.blk[1] = mkblock("ftou_big");

	maxflt = mkfltconst(v->repr, 0x1p63);
	maxint = mkintconst(&i64, 1ull<<63);

	big = funcinst(f, v->repr->base == 's' ? ICGES : ICGED, &i32, v, maxflt);
	funcjnz(f, big, join->phi.blk[1], join->phi.blk[0]);

	funclabel(f, join->phi.blk[0]);
	join->phi.val[0] = funcinst(f, op, r, v, NULL);
	funcjmp(f, join);

	funclabel(f, join->phi.blk[1]);
	v = funcinst(f, ISUB, v->repr, v, maxflt);
	v = funcinst(f, op, r, v, NULL);
	join->phi.val[1] = funcinst(f, IXOR, r, v, maxint);

	funclabel(f, join);
	functemp(f, &join->phi.res, r);
	return &join->phi.res;
}

static struct value *
extend(struct func *f, struct type *t, struct value *v)
{
	enum instkind op;

	switch (t->size) {
	case 1: op = t->basic.issigned ? IEXTSB : IEXTUB; break;
	case 2: op = t->basic.issigned ? IEXTSH : IEXTUH; break;
	default: return v;
	}
	return funcinst(f, op, &i32, v, NULL);
}

static struct value *
convert(struct func *f, struct type *dst, struct type *src, struct value *l)
{
	enum instkind op;
	struct value *r = NULL;

	if (src->kind == TYPEPOINTER)
		src = &typeulong;
	if (dst->kind == TYPEPOINTER)
		dst = &typeulong;
	if (dst->kind == TYPEVOID)
		return NULL;
	if (!(src->prop & PROPREAL) || !(dst->prop & PROPREAL))
		fatal("internal error; unsupported conversion");
	if (dst->kind == TYPEBOOL) {
		l = extend(f, src, l);
		r = mkintconst(src->repr, 0);
		if (src->prop & PROPINT)
			op = src->size == 8 ? ICNEL : ICNEW;
		else
			op = src->size == 8 ? ICNED : ICNES;
	} else if (dst->prop & PROPINT) {
		if (src->prop & PROPINT) {
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
		if (src->prop & PROPINT) {
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

	return funcinst(f, op, dst->repr, l, r);
}

struct func *
mkfunc(struct decl *decl, char *name, struct type *t, struct scope *s)
{
	struct func *f;
	struct param *p;
	struct decl *d;
	struct type *pt;
	struct value *v;

	f = xmalloc(sizeof(*f));
	f->decl = decl;
	f->name = name;
	f->type = t;
	f->start = f->end = mkblock("start");
	f->gotos = mkmap(8);
	f->lastid = 0;
	emittype(t->base);

	/* allocate space for parameters */
	for (p = t->func.params; p; p = p->next) {
		if (!p->name)
			error(&tok.loc, "parameter name omitted in definition of function '%s'", name);
		pt = t->func.isprototype ? p->type : typepromote(p->type, -1);
		emittype(pt);
		p->value = xmalloc(sizeof(*p->value));
		functemp(f, p->value, pt->repr);
		d = mkdecl(DECLOBJECT, p->type, p->qual, LINKNONE);
		if (p->type->value) {
			d->value = p->value;
		} else {
			v = typecompatible(p->type, pt) ? p->value : convert(f, pt, p->type, p->value);
			funcinit(f, d, NULL);
			funcstore(f, p->type, QUALNONE, (struct lvalue){d->value}, v);
		}
		scopeputdecl(s, p->name, d);
	}

	t = mkarraytype(&typechar, QUALCONST, strlen(name) + 1);
	d = mkdecl(DECLOBJECT, t, QUALNONE, LINKNONE);
	d->value = mkglobal("__func__", true);
	scopeputdecl(s, "__func__", d);
	/*
	needed for glibc's assert definition with __GNUC__=2 __GNUC_MINOR__=4
	XXX: this should also work at file scope, where it should evaluate to "toplevel"
	*/
	scopeputdecl(s, "__PRETTY_FUNCTION__", d);
	f->namedecl = d;

	funclabel(f, mkblock("body"));

	return f;
}

void
delfunc(struct func *f)
{
	struct block *b;
	struct inst **inst;

	while (b = f->start) {
		f->start = b->next;
		arrayforeach (&b->insts, inst)
			free(*inst);
		free(b->insts.val);
		free(b);
	}
	delmap(f->gotos, free);
	free(f);
}

struct type *
functype(struct func *f)
{
	return f->type;
}

void
funclabel(struct func *f, struct block *b)
{
	f->end->next = b;
	f->end = b;
}

void
funcjmp(struct func *f, struct block *l)
{
	struct block *b = f->end;

	if (!b->jump.kind) {
		b->jump.kind = JUMP_JMP;
		b->jump.blk[0] = l;
	}
}

void
funcjnz(struct func *f, struct value *v, struct block *l1, struct block *l2)
{
	struct block *b = f->end;

	if (!b->jump.kind) {
		b->jump.kind = JUMP_JNZ;
		b->jump.arg = v;
		b->jump.blk[0] = l1;
		b->jump.blk[1] = l2;
	}
}

void
funcret(struct func *f, struct value *v)
{
	struct block *b = f->end;

	if (!b->jump.kind) {
		b->jump.kind = JUMP_RET;
		b->jump.arg = v;
	}
}

struct gotolabel *
funcgoto(struct func *f, char *name)
{
	void **entry;
	struct gotolabel *g;
	struct mapkey key;

	mapkey(&key, name, strlen(name));
	entry = mapput(f->gotos, &key);
	g = *entry;
	if (!g) {
		g = xmalloc(sizeof(*g));
		g->label = mkblock(name);
		*entry = g;
	}

	return g;
}

static struct lvalue
funclval(struct func *f, struct expr *e)
{
	struct lvalue lval = {0};
	struct decl *d;

	if (e->kind == EXPRBITFIELD) {
		lval.bits = e->bitfield.bits;
		e = e->base;
	}
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
		lval.addr = d->value;
		break;
	case EXPRSTRING:
		d = stringdecl(e);
		lval.addr = d->value;
		break;
	case EXPRCOMPOUND:
		d = mkdecl(DECLOBJECT, e->type, e->qual, LINKNONE);
		funcinit(f, d, e->compound.init);
		lval.addr = d->value;
		break;
	case EXPRUNARY:
		if (e->op != TMUL)
			error(&tok.loc, "expression is not an object");
		lval.addr = funcexpr(f, e->base);
		break;
	default:
		if (e->type->kind != TYPESTRUCT && e->type->kind != TYPEUNION)
			error(&tok.loc, "expression is not an object");
		lval.addr = funcinst(f, ICOPY, &iptr, funcexpr(f, e), NULL);
	}
	return lval;
}

struct value *
funcexpr(struct func *f, struct expr *e)
{
	enum instkind op = INONE;
	struct decl *d;
	struct value *l, *r, *v, **argvals;
	struct lvalue lval;
	struct expr *arg;
	struct block *b[3];
	struct type *t;
	size_t i;

	switch (e->kind) {
	case EXPRIDENT:
		d = e->ident.decl;
		switch (d->kind) {
		case DECLOBJECT: return funcload(f, d->type, (struct lvalue){d->value});
		case DECLCONST:  return d->value;
		default:
			fatal("unimplemented declaration kind %d", d->kind);
		}
		break;
	case EXPRCONST:
		if (e->type->prop & PROPINT || e->type->kind == TYPEPOINTER)
			return mkintconst(e->type->repr, e->constant.i);
		return mkfltconst(e->type->repr, e->constant.f);
	case EXPRBITFIELD:
	case EXPRCOMPOUND:
		lval = funclval(f, e);
		return funcload(f, e->type, lval);
	case EXPRINCDEC:
		lval = funclval(f, e->base);
		l = funcload(f, e->base->type, lval);
		if (e->type->kind == TYPEPOINTER)
			r = mkintconst(e->type->repr, e->type->base->size);
		else if (e->type->prop & PROPINT)
			r = mkintconst(e->type->repr, 1);
		else if (e->type->prop & PROPFLOAT)
			r = mkfltconst(e->type->repr, 1);
		else
			fatal("not a scalar");
		v = funcinst(f, e->op == TINC ? IADD : ISUB, e->type->repr, l, r);
		v = funcstore(f, e->type, e->qual, lval, v);
		return e->incdec.post ? l : v;
	case EXPRCALL:
		op = e->base->type->base->func.isvararg ? IVACALL : ICALL;
		argvals = xreallocarray(NULL, e->call.nargs, sizeof(argvals[0]));
		for (arg = e->call.args, i = 0; arg; arg = arg->next, ++i) {
			emittype(arg->type);
			argvals[i] = funcexpr(f, arg);
		}
		emittype(e->type);
		v = funcinst(f, op, e->type->repr, funcexpr(f, e->base), e->type->value);
		for (arg = e->call.args, i = 0; arg; arg = arg->next, ++i)
			funcinst(f, IARG, NULL, argvals[i], arg->type->value);
		//if (e->base->type->base->func.isnoreturn)
		//	funcret(f, NULL);
		return v;
	case EXPRUNARY:
		switch (e->op) {
		case TBAND:
			lval = funclval(f, e->base);
			return lval.addr;
		case TMUL:
			r = funcexpr(f, e->base);
			return funcload(f, e->type, (struct lvalue){r});
		}
		fatal("internal error; unknown unary expression");
		break;
	case EXPRCAST:
		l = funcexpr(f, e->base);
		return convert(f, e->type, e->base->type, l);
	case EXPRBINARY:
		l = funcexpr(f, e->binary.l);
		if (e->op == TLOR || e->op == TLAND) {
			b[0] = mkblock("logic_right");
			b[1] = mkblock("logic_join");
			if (e->op == TLOR)
				funcjnz(f, l, b[1], b[0]);
			else
				funcjnz(f, l, b[0], b[1]);
			b[1]->phi.val[0] = l;
			b[1]->phi.blk[0] = f->end;
			funclabel(f, b[0]);
			r = funcexpr(f, e->binary.r);
			b[1]->phi.val[1] = r;
			b[1]->phi.blk[1] = f->end;
			funclabel(f, b[1]);
			functemp(f, &b[1]->phi.res, e->type->repr);
			return &b[1]->phi.res;
		}
		r = funcexpr(f, e->binary.r);
		t = e->binary.l->type;
		if (t->kind == TYPEPOINTER)
			t = &typeulong;
		switch (e->op) {
		case TMUL:
			op = IMUL;
			break;
		case TDIV:
			op = !(e->type->prop & PROPINT) || e->type->basic.issigned ? IDIV : IUDIV;
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
				op = t->prop & PROPFLOAT ? ICLTS : t->basic.issigned ? ICSLTW : ICULTW;
			else
				op = t->prop & PROPFLOAT ? ICLTD : t->basic.issigned ? ICSLTL : ICULTL;
			break;
		case TGREATER:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = t->prop & PROPFLOAT ? ICGTS : t->basic.issigned ? ICSGTW : ICUGTW;
			else
				op = t->prop & PROPFLOAT ? ICGTD : t->basic.issigned ? ICSGTL : ICUGTL;
			break;
		case TLEQ:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = t->prop & PROPFLOAT ? ICLES : t->basic.issigned ? ICSLEW : ICULEW;
			else
				op = t->prop & PROPFLOAT ? ICLED : t->basic.issigned ? ICSLEL : ICULEL;
			break;
		case TGEQ:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = t->prop & PROPFLOAT ? ICGES : t->basic.issigned ? ICSGEW : ICUGEW;
			else
				op = t->prop & PROPFLOAT ? ICGED : t->basic.issigned ? ICSGEL : ICUGEL;
			break;
		case TEQL:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = t->prop & PROPFLOAT ? ICEQS : ICEQW;
			else
				op = t->prop & PROPFLOAT ? ICEQD : ICEQL;
			break;
		case TNEQ:
			l = extend(f, t, l);
			r = extend(f, t, r);
			if (t->size <= 4)
				op = t->prop & PROPFLOAT ? ICNES : ICNEW;
			else
				op = t->prop & PROPFLOAT ? ICNED : ICNEL;
			break;
		}
		if (op == INONE)
			fatal("internal error; unimplemented binary expression");
		return funcinst(f, op, e->type->repr, l, r);
	case EXPRCOND:
		b[0] = mkblock("cond_true");
		b[1] = mkblock("cond_false");
		b[2] = mkblock("cond_join");

		v = funcexpr(f, e->base);
		funcjnz(f, v, b[0], b[1]);

		funclabel(f, b[0]);
		b[2]->phi.val[0] = funcexpr(f, e->cond.t);
		b[2]->phi.blk[0] = f->end;
		funcjmp(f, b[2]);

		funclabel(f, b[1]);
		b[2]->phi.val[1] = funcexpr(f, e->cond.f);
		b[2]->phi.blk[1] = f->end;

		funclabel(f, b[2]);
		if (e->type == &typevoid)
			return NULL;
		functemp(f, &b[2]->phi.res, e->type->repr);
		return &b[2]->phi.res;
	case EXPRASSIGN:
		r = funcexpr(f, e->assign.r);
		if (e->assign.l->kind == EXPRTEMP) {
			e->assign.l->temp = r;
		} else {
			lval = funclval(f, e->assign.l);
			r = funcstore(f, e->assign.l->type, e->assign.l->qual, lval, r);
		}
		return r;
	case EXPRCOMMA:
		for (e = e->base; e->next; e = e->next)
			funcexpr(f, e);
		return funcexpr(f, e);
	case EXPRBUILTIN:
		switch (e->builtin.kind) {
		case BUILTINVASTART:
			l = funcexpr(f, e->base);
			funcinst(f, IVASTART, NULL, l, NULL);
			break;
		case BUILTINVAARG:
			/* https://todo.sr.ht/~mcf/cproc/52 */
			if (!(e->type->prop & PROPSCALAR))
				error(&tok.loc, "va_arg with non-scalar type is not yet supported");
			l = funcexpr(f, e->base);
			return funcinst(f, IVAARG, e->type->repr, l, NULL);
		case BUILTINVAEND:
			/* no-op */
			break;
		case BUILTINALLOCA:
			l = funcexpr(f, e->base);
			return funcinst(f, IALLOC16, &iptr, l, NULL);
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
zero(struct func *func, struct value *addr, int align, uint64_t offset, uint64_t end)
{
	enum instkind store[] = {
		[1] = ISTOREB,
		[2] = ISTOREH,
		[4] = ISTOREW,
		[8] = ISTOREL,
	};
	struct value *tmp;
	static struct value z = {.kind = VALUE_CONST, .repr = &i64};
	int a = 1;

	while (offset < end) {
		if ((align - (offset & align - 1)) & a) {
			tmp = offset ? funcinst(func, IADD, &iptr, addr, mkintconst(&iptr, offset)) : addr;
			funcinst(func, store[a], NULL, &z, tmp);
			offset += a;
		}
		if (a < align)
			a <<= 1;
	}
}

void
funcinit(struct func *func, struct decl *d, struct init *init)
{
	struct lvalue dst;
	struct value *src;
	uint64_t offset = 0, max = 0;
	size_t i;

	funcalloc(func, d);
	if (!init)
		return;
	for (; init; init = init->next) {
		zero(func, d->value, d->type->align, offset, init->start);
		dst.bits = init->bits;
		if (init->expr->kind == EXPRSTRING) {
			for (i = 0; i < init->expr->string.size && i < init->end - init->start; ++i) {
				dst.addr = funcinst(func, IADD, &iptr, d->value, mkintconst(&iptr, init->start + i));
				funcstore(func, &typechar, QUALNONE, dst, mkintconst(&i8, init->expr->string.data[i]));
			}
			offset = init->start + i;
		} else {
			if (offset < init->end && (dst.bits.before || dst.bits.after))
				zero(func, d->value, d->type->align, offset, init->end);
			dst.addr = d->value;
			/*
			QBE's memopt does not eliminate the store for ptr + 0,
			so only emit the add if the offset is non-zero
			*/
			if (init->start > 0)
				dst.addr = funcinst(func, IADD, &iptr, dst.addr, mkintconst(&iptr, init->start));
			src = funcexpr(func, init->expr);
			funcstore(func, init->expr->type, QUALNONE, dst, src);
			offset = init->end;
		}
		if (max < offset)
			max = offset;
	}
	zero(func, d->value, d->type->align, max, d->type->size);
}

static void
casesearch(struct func *f, struct value *v, struct switchcase *c, struct block *defaultlabel)
{
	struct value *res, *key;
	struct block *label[3];

	if (!c) {
		funcjmp(f, defaultlabel);
		return;
	}
	label[0] = mkblock("switch_ne");
	label[1] = mkblock("switch_lt");
	label[2] = mkblock("switch_gt");

	// XXX: linear search if c->node.height < 4
	key = mkintconst(v->repr, c->node.key);
	res = funcinst(f, v->repr->base == 'w' ? ICEQW : ICEQL, &i32, v, key);
	funcjnz(f, res, c->body, label[0]);
	funclabel(f, label[0]);
	res = funcinst(f, v->repr->base == 'w' ? ICULTW : ICULTL, &i32, v, key);
	funcjnz(f, res, label[1], label[2]);
	funclabel(f, label[1]);
	casesearch(f, v, c->node.child[0], defaultlabel);
	funclabel(f, label[2]);
	casesearch(f, v, c->node.child[1], defaultlabel);
}

void
funcswitch(struct func *f, struct value *v, struct switchcases *c, struct block *defaultlabel)
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
		[VALUE_TEMP] = '%',
		[VALUE_GLOBAL] = '$',
		[VALUE_TYPE] = ':',
	};

	switch (v->kind) {
	case VALUE_CONST:
		if (v->repr->base == 's' || v->repr->base == 'd')
			printf("%c_%.*g", v->repr->base, DECIMAL_DIG, v->f);
		else
			printf("%" PRIu64, v->i);
		break;
	default:
		if (v->kind >= LEN(sigil) || !sigil[v->kind])
			fatal("invalid value");
		putchar(sigil[v->kind]);
		if (v->kind == VALUE_GLOBAL && v->name.id)
			fputs(".L", stdout);
		emitname(&v->name);
	}
}

static void
emitrepr(struct repr *r, struct value *v, bool ext)
{
	if (!r)
		fatal("type has no QBE representation");
	if (v && v->kind == VALUE_TYPE)
		emitvalue(v);
	else
		putchar(ext ? r->ext : r->base);
}

/* XXX: need to consider _Alignas on struct members */
static void
emittype(struct type *t)
{
	static uint64_t id;
	struct member *m, *other;
	struct type *sub;
	uint64_t i, off;

	if (t->value || !t->repr || t->kind != TYPESTRUCT && t->kind != TYPEUNION)
		return;
	t->value = xmalloc(sizeof(*t->value));
	t->value->kind = VALUE_TYPE;
	t->value->name.str = t->structunion.tag;
	t->value->name.id = ++id;
	for (m = t->structunion.members; m; m = m->next) {
		for (sub = m->type; sub->kind == TYPEARRAY; sub = sub->base)
			;
		emittype(sub);
	}
	fputs("type ", stdout);
	emitvalue(t->value);
	fputs(" = { ", stdout);
	for (m = t->structunion.members, off = 0; m;) {
		if (t->kind == TYPESTRUCT) {
			/* look for a subsequent member with a larger storage unit */
			for (other = m->next; other && other->offset < ALIGNUP(m->offset + 1, 8); other = other->next) {
				if (other->offset <= m->offset)
					m = other;
			}
			off = m->offset + m->type->size;
		} else {
			fputs("{ ", stdout);
		}
		for (i = 1, sub = m->type; sub->kind == TYPEARRAY; sub = sub->base)
			i *= sub->array.length;
		emitrepr(sub->repr, sub->value, true);
		if (i > 1)
			printf(" %" PRIu64, i);
		if (t->kind == TYPESTRUCT) {
			fputs(", ", stdout);
			/* skip subsequent members contained within the same storage unit */
			do m = m->next;
			while (m && m->offset < off);
		} else {
			fputs(" } ", stdout);
			m = m->next;
		}
	}
	puts("}");
}

static struct inst **
emitinst(struct inst **instp, struct inst **instend)
{
	int op, first;
	struct inst *inst = *instp;

	putchar('\t');
	assert(inst->kind < LEN(instname));
	if (inst->res.kind) {
		emitvalue(&inst->res);
		fputs(" =", stdout);
		emitrepr(inst->res.repr, inst->arg[1], false);
		putchar(' ');
	}
	fputs(instname[inst->kind], stdout);
	putchar(' ');
	emitvalue(inst->arg[0]);
	++instp;
	op = inst->kind;
	switch (op) {
	case ICALL:
	case IVACALL:
		putchar('(');
		for (first = 1; instp != instend && (*instp)->kind == IARG; ++instp) {
			if (first)
				first = 0;
			else
				fputs(", ", stdout);
			inst = *instp;
			emitrepr(inst->arg[0]->repr, inst->arg[1], false);
			putchar(' ');
			emitvalue(inst->arg[0]);
		}
		if (op == IVACALL)
			fputs(", ...", stdout);
		putchar(')');
		break;
	default:
		if (inst->arg[1]) {
			fputs(", ", stdout);
			emitvalue(inst->arg[1]);
		}
	}
	putchar('\n');
	return instp;
}

static void
emitjump(struct jump *j)
{
	switch (j->kind) {
	case JUMP_RET:
		fputs("\tret", stdout);
		if (j->arg) {
			fputc(' ', stdout);
			emitvalue(j->arg);
		}
		putchar('\n');
		break;
	case JUMP_JMP:
		fputs("\tjmp @", stdout);
		emitname(&j->blk[0]->label);
		putchar('\n');
		break;
	case JUMP_JNZ:
		fputs("\tjnz ", stdout);
		emitvalue(j->arg);
		fputs(", @", stdout);
		emitname(&j->blk[0]->label);
		fputs(", @", stdout);
		emitname(&j->blk[1]->label);
		putchar('\n');
		break;
	}
}

void
emitfunc(struct func *f, bool global)
{
	struct block *b;
	struct inst **inst, **instend;
	struct param *p;

	if (f->end->jump.kind == JUMP_NONE)
		funcret(f, strcmp(f->name, "main") == 0 ? mkintconst(&i32, 0) : NULL);
	if (global)
		puts("export");
	fputs("function ", stdout);
	if (f->type->base != &typevoid) {
		emitrepr(f->type->base->repr, f->type->base->value, false);
		putchar(' ');
	}
	emitvalue(f->decl->value);
	putchar('(');
	for (p = f->type->func.params; p; p = p->next) {
		if (p != f->type->func.params)
			fputs(", ", stdout);
		emitrepr(p->value->repr, p->type->value, false);
		putchar(' ');
		emitvalue(p->value);
	}
	if (f->type->func.isvararg)
		fputs(", ...", stdout);
	puts(") {");
	for (b = f->start; b; b = b->next) {
		putchar('@');
		emitname(&b->label);
		putchar('\n');
		if (b->phi.res.kind) {
			putchar('\t');
			emitvalue(&b->phi.res);
			printf(" =%c phi @", b->phi.res.repr->base);
			emitname(&b->phi.blk[0]->label);
			putchar(' ');
			emitvalue(b->phi.val[0]);
			fputs(", @", stdout);
			emitname(&b->phi.blk[1]->label);
			putchar(' ');
			emitvalue(b->phi.val[1]);
			putchar('\n');
		}
		instend = (struct inst **)((char *)b->insts.val + b->insts.len);
		for (inst = b->insts.val; inst != instend;)
			inst = emitinst(inst, instend);
		emitjump(&b->jump);
	}
	puts("}");
}

static void
dataitem(struct expr *expr, uint64_t size)
{
	struct decl *decl;
	size_t i;
	char c;

	switch (expr->kind) {
	case EXPRUNARY:
		if (expr->op != TBAND)
			fatal("not a address expr");
		expr = expr->base;
		if (expr->kind != EXPRIDENT)
			error(&tok.loc, "initializer is not a constant expression");
		decl = expr->ident.decl;
		if (decl->value->kind != VALUE_GLOBAL)
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
		if (expr->type->prop & PROPFLOAT)
			printf("%c_%.*g", expr->type->size == 4 ? 's' : 'd', DECIMAL_DIG, expr->constant.f);
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
		error(&tok.loc, "initializer is not a constant expression");
	}
}

void
emitdata(struct decl *d, struct init *init)
{
	struct init *cur;
	uint64_t offset = 0, start, end, bits = 0;

	if (!d->align)
		d->align = d->type->align;
	else if (d->align < d->type->align)
		error(&tok.loc, "object requires alignment %d, which is stricter than %d", d->type->align, d->align);
	for (cur = init; cur; cur = cur->next)
		cur->expr = eval(cur->expr, EVALINIT);
	if (d->linkage == LINKEXTERN)
		fputs("export ", stdout);
	fputs("data ", stdout);
	emitvalue(d->value);
	printf(" = align %d { ", d->align);

	while (init) {
		cur = init;
		while ((init = init->next) && init->start * 8 + init->bits.before < cur->end * 8 - cur->bits.after) {
			/*
			XXX: Currently, if multiple union members are
			initialized, these assertions may not hold.
			(https://todo.sr.ht/~mcf/cproc/38)
			*/
			assert(cur->expr->kind == EXPRSTRING);
			assert(init->expr->kind == EXPRCONST);
			cur->expr->string.data[init->start - cur->start] = init->expr->constant.i;
		}
		start = cur->start + cur->bits.before / 8;
		end = cur->end - (cur->bits.after + 7) / 8;
		if (offset < start && bits) {
			printf("b %u, ", (unsigned)bits);  /* unfinished byte from previous bit-field */
			++offset;
			bits = 0;
		}
		if (offset < start)
			printf("z %" PRIu64 ", ", start - offset);
		if (cur->bits.before || cur->bits.after) {
			/* XXX: little-endian specific */
			assert(cur->expr->type->prop & PROPINT);
			assert(cur->expr->kind == EXPRCONST);
			bits |= cur->expr->constant.i << cur->bits.before % 8;
			for (offset = start; offset < end; ++offset, bits >>= 8)
				printf("b %u, ", (unsigned)bits & 0xff);
			/*
			clear the upper `after` bits in the last byte,
			or all bits when `after` is 0 (we ended on a
			byte boundary).
			*/
			bits &= 0x7f >> (cur->bits.after + 7) % 8;
		} else {
			printf("%c ", cur->expr->type->kind == TYPEARRAY ? cur->expr->type->base->repr->ext : cur->expr->type->repr->ext);
			dataitem(cur->expr, cur->end - cur->start);
			fputs(", ", stdout);
		}
		offset = end;
	}
	if (bits) {
		printf("b %u, ", (unsigned)bits);
		++offset;
	}
	assert(offset <= d->type->size);
	if (offset < d->type->size)
		printf("z %" PRIu64 " ", d->type->size - offset);
	puts("}");
}
