#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "util.h"
#include "cc.h"

#define F (1<<8)
#define S (2<<8)
static void
cast(struct expr *expr)
{
	unsigned size;
	unsigned long long m;

	size = expr->type->size;
	if (expr->type->prop & PROPFLOAT) {
		if (size == 4)
			expr->u.constant.f = (float)expr->u.constant.f;
	} else if (expr->type->prop & PROPINT) {
		expr->u.constant.u &= -1ull >> CHAR_BIT * sizeof(unsigned long long) - size * 8;
		if (expr->type->u.basic.issigned) {
			m = 1ull << size * 8 - 1;
			expr->u.constant.u = (expr->u.constant.u ^ m) - m;
		}
	}
}

static void
unary(struct expr *expr, enum tokenkind op, struct expr *l)
{
	expr->kind = EXPRCONST;
	if (l->type->prop & PROPFLOAT)
		op |= F;
	switch (op) {
	case TSUB:      expr->u.constant.u = -l->u.constant.u; break;
	case TSUB|F:    expr->u.constant.f = -l->u.constant.f; break;
	default:
		fatal("internal error; unknown unary expression");
	}
	cast(expr);
}

static void
binary(struct expr *expr, enum tokenkind op, struct expr *l, struct expr *r)
{
	expr->kind = EXPRCONST;
	if (l->type->prop & PROPFLOAT)
		op |= F;
	else if (l->type->prop & PROPINT && l->type->u.basic.issigned)
		op |= S;
	switch (op) {
	case TMUL:
	case TMUL|S:     expr->u.constant.u = l->u.constant.u * r->u.constant.u; break;
	case TMUL|F:     expr->u.constant.f = l->u.constant.f * r->u.constant.f; break;
	case TDIV:       expr->u.constant.u = l->u.constant.u / r->u.constant.u; break;
	case TDIV|S:     expr->u.constant.i = l->u.constant.i / r->u.constant.i; break;
	case TDIV|F:     expr->u.constant.f = l->u.constant.f / r->u.constant.f; break;
	case TMOD:       expr->u.constant.u = l->u.constant.u % r->u.constant.u; break;
	case TMOD|S:     expr->u.constant.i = l->u.constant.i % r->u.constant.i; break;
	case TADD:
	case TADD|S:     expr->u.constant.u = l->u.constant.u + r->u.constant.u; break;
	case TADD|F:     expr->u.constant.f = l->u.constant.f + r->u.constant.f; break;
	case TSUB:
	case TSUB|S:     expr->u.constant.u = l->u.constant.u - r->u.constant.u; break;
	case TSUB|F:     expr->u.constant.f = l->u.constant.f - r->u.constant.f; break;
	case TSHL:
	case TSHL|S:     expr->u.constant.u = l->u.constant.u << (r->u.constant.u & 63); break;
	case TSHR:       expr->u.constant.u = l->u.constant.u >> (r->u.constant.u & 63); break;
	case TSHR|S:     expr->u.constant.i = l->u.constant.i >> (r->u.constant.u & 63); break;
	case TBAND:
	case TBAND|S:    expr->u.constant.u = l->u.constant.u & r->u.constant.u; break;
	case TBOR:
	case TBOR|S:     expr->u.constant.u = l->u.constant.u | r->u.constant.u; break;
	case TXOR:
	case TXOR|S:     expr->u.constant.u = l->u.constant.u ^ r->u.constant.u; break;
	case TLESS:      expr->u.constant.u = l->u.constant.u < r->u.constant.u; break;
	case TLESS|S:    expr->u.constant.u = l->u.constant.i < r->u.constant.i; break;
	case TLESS|F:    expr->u.constant.u = l->u.constant.f < r->u.constant.f; break;
	case TGREATER:   expr->u.constant.u = l->u.constant.u > r->u.constant.u; break;
	case TGREATER|S: expr->u.constant.u = l->u.constant.i > r->u.constant.i; break;
	case TGREATER|F: expr->u.constant.u = l->u.constant.f > r->u.constant.f; break;
	case TLEQ:       expr->u.constant.u = l->u.constant.u <= r->u.constant.u; break;
	case TLEQ|S:     expr->u.constant.u = l->u.constant.i <= r->u.constant.i; break;
	case TLEQ|F:     expr->u.constant.u = l->u.constant.f <= r->u.constant.f; break;
	case TGEQ:       expr->u.constant.u = l->u.constant.u >= r->u.constant.u; break;
	case TGEQ|S:     expr->u.constant.u = l->u.constant.i >= r->u.constant.i; break;
	case TGEQ|F:     expr->u.constant.u = l->u.constant.f >= r->u.constant.f; break;
	case TEQL:
	case TEQL|S:     expr->u.constant.u = l->u.constant.u == r->u.constant.u; break;
	case TEQL|F:     expr->u.constant.u = l->u.constant.f == r->u.constant.f; break;
	case TNEQ:
	case TNEQ|S:     expr->u.constant.u = l->u.constant.u != r->u.constant.u; break;
	case TNEQ|F:     expr->u.constant.u = l->u.constant.f != r->u.constant.f; break;
	default:
		fatal("internal error; unknown binary expression");
	}
	cast(expr);
}
#undef F
#undef S

struct expr *
eval(struct expr *expr, enum evalkind kind)
{
	struct expr *l, *r, *c;
	struct decl *d;
	struct type *t;

	t = expr->type;
	switch (expr->kind) {
	case EXPRIDENT:
		if (expr->u.ident.decl->kind != DECLCONST)
			break;
		expr->kind = EXPRCONST;
		expr->u.constant.u = intconstvalue(expr->u.ident.decl->value);
		break;
	case EXPRCOMPOUND:
		if (kind != EVALINIT)
			break;
		d = mkdecl(DECLOBJECT, t, expr->qual, LINKNONE);
		d->value = mkglobal(NULL, true);
		emitdata(d, expr->u.compound.init);
		expr->kind = EXPRIDENT;
		expr->u.ident.decl = d;
		break;
	case EXPRUNARY:
		l = eval(expr->base, kind);
		switch (expr->op) {
		case TBAND:
			switch (l->kind) {
			case EXPRUNARY:
				if (l->op == TMUL)
					expr = eval(l->base, kind);
				break;
			case EXPRSTRING:
				if (kind != EVALINIT)
					break;
				l->u.ident.decl = stringdecl(l);
				l->kind = EXPRIDENT;
				expr->base = l;
				break;
			}
			break;
		case TMUL:
			break;
		default:
			if (l->kind != EXPRCONST)
				break;
			unary(expr, expr->op, l);
			break;
		}
		break;
	case EXPRCAST:
		l = eval(expr->base, kind);
		if (l->kind == EXPRCONST) {
			expr->kind = EXPRCONST;
			if (l->type->prop & PROPINT && t->prop & PROPFLOAT) {
				if (l->type->u.basic.issigned)
					expr->u.constant.f = l->u.constant.i;
				else
					expr->u.constant.f = l->u.constant.u;
			} else if (l->type->prop & PROPFLOAT && t->prop & PROPINT) {
				if (t->u.basic.issigned) {
					if (l->u.constant.f < -0x1p63 || l->u.constant.f >= 0x1p63)
						error(&tok.loc, "integer part of floating-point constant %g cannot be represented as signed integer", l->u.constant.f);
					expr->u.constant.i = l->u.constant.f;
				} else {
					if (l->u.constant.f < 0.0 || l->u.constant.f >= 0x1p64)
						error(&tok.loc, "integer part of floating-point constant %g cannot be represented as unsigned integer", l->u.constant.f);
					expr->u.constant.u = l->u.constant.f;
				}
			} else {
				expr->u.constant = l->u.constant;
			}
			cast(expr);
		} else if (l->type->kind == TYPEPOINTER) {
			/*
			A cast from a pointer to integer is not a valid constant
			expression, but C11 allows implementations to recognize
			other forms of constant expressions (6.6p10), and some
			programs expect this functionality.
			*/
			if (t->kind == TYPEPOINTER || t->prop & PROPINT && t->size == typelong.size)
				expr = l;
		}
		break;
	case EXPRBINARY:
		l = eval(expr->u.binary.l, kind);
		r = eval(expr->u.binary.r, kind);
		expr->u.binary.l = l;
		expr->u.binary.r = r;
		switch (expr->op) {
		case TADD:
			if (r->kind == EXPRBINARY)
				c = l, l = r, r = c;
			/* fallthrough */
		case TSUB:
			if (r->kind != EXPRCONST)
				break;
			if (l->kind == EXPRCONST) {
				binary(expr, expr->op, l, r);
			} else if (l->kind == EXPRBINARY && l->type->kind == TYPEPOINTER && l->op == TADD && l->u.binary.r->kind == EXPRCONST) {
				/* (P + C1) ± C2  ->  P + (C1 ± C2) */
				binary(expr->u.binary.r, expr->op, l->u.binary.r, r);
				expr->op = TADD;
				expr->u.binary.l = l->u.binary.l;
			}
			break;
		case TLOR:
			if (l->kind != EXPRCONST)
				break;
			return l->u.constant.u ? l : r;
		case TLAND:
			if (l->kind != EXPRCONST)
				break;
			return l->u.constant.u ? r : l;
		default:
			if (l->kind != EXPRCONST || r->kind != EXPRCONST)
				break;
			binary(expr, expr->op, l, r);
		}
		break;
	}

	return expr;
}
