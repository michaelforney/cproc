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

	size = expr->type->size;
	if (expr->type->prop & PROPFLOAT)
		size |= F;
	else if (expr->type->basic.issigned)
		size |= S;
	switch (size) {
	case 1:   expr->constant.i = (uint8_t)expr->constant.i;  break;
	case 1|S: expr->constant.i = (int8_t)expr->constant.i;   break;
	case 2:   expr->constant.i = (uint16_t)expr->constant.i; break;
	case 2|S: expr->constant.i = (int16_t)expr->constant.i;  break;
	case 4:   expr->constant.i = (uint32_t)expr->constant.i; break;
	case 4|S: expr->constant.i = (int32_t)expr->constant.i;  break;
	case 4|F: expr->constant.f = (float)expr->constant.f;    break;
	}
}

static void
binary(struct expr *expr, enum tokenkind op, struct expr *l, struct expr *r)
{
	expr->kind = EXPRCONST;
	if (l->type->prop & PROPFLOAT)
		op |= F;
	else if (l->type->basic.issigned)
		op |= S;
	switch (op) {
	case TMUL:
	case TMUL|S:    expr->constant.i = l->constant.i * r->constant.i; break;
	case TMUL|F:    expr->constant.f = l->constant.f * r->constant.f; break;
	case TDIV:      expr->constant.i = l->constant.i / r->constant.i; break;
	case TDIV|S:    expr->constant.i = (int64_t)l->constant.i / (int64_t)r->constant.i; break;
	case TDIV|F:    expr->constant.f = l->constant.f / r->constant.f; break;
	case TMOD:      expr->constant.i = l->constant.i % r->constant.i; break;
	case TMOD|S:    expr->constant.i = (int64_t)l->constant.i % (int64_t)r->constant.i; break;
	case TADD:
	case TADD|S:    expr->constant.i = l->constant.i + r->constant.i; break;
	case TADD|F:    expr->constant.f = l->constant.f + r->constant.f; break;
	case TSUB:
	case TSUB|S:    expr->constant.i = l->constant.i - r->constant.i; break;
	case TSUB|F:    expr->constant.f = l->constant.f - r->constant.f; break;
	case TSHL:
	case TSHL|S:    expr->constant.i = l->constant.i << r->constant.i; break;
	case TSHR:      expr->constant.i = l->constant.i >> r->constant.i; break;
	case TSHR|S:    expr->constant.i = (int64_t)l->constant.i >> r->constant.i; break;
	case TBAND:
	case TBAND|S:   expr->constant.i = l->constant.i & r->constant.i; break;
	case TBOR:
	case TBOR|S:    expr->constant.i = l->constant.i | r->constant.i; break;
	case TXOR:
	case TXOR|S:    expr->constant.i = l->constant.i ^ r->constant.i; break;
	case TLESS:     expr->constant.i = l->constant.i < r->constant.i; break;
	case TLESS|S:   expr->constant.i = (int64_t)l->constant.i < (int64_t)r->constant.i; break;
	case TLESS|F:   expr->constant.i = l->constant.f < r->constant.f; break;
	case TGREATER:  expr->constant.i = l->constant.i > r->constant.i; break;
	case TGREATER|S: expr->constant.i = (int64_t)l->constant.i > (int64_t)r->constant.i; break;
	case TGREATER|F: expr->constant.i = l->constant.f > r->constant.f; break;
	case TLEQ:      expr->constant.i = l->constant.i <= r->constant.i; break;
	case TLEQ|S:    expr->constant.i = (int64_t)l->constant.i <= (int64_t)r->constant.i; break;
	case TLEQ|F:    expr->constant.i = l->constant.f <= r->constant.f; break;
	case TGEQ:      expr->constant.i = l->constant.i >= r->constant.i; break;
	case TGEQ|S:    expr->constant.i = (int64_t)l->constant.i >= (int64_t)r->constant.i; break;
	case TGEQ|F:    expr->constant.i = l->constant.f >= r->constant.f; break;
	case TEQL:
	case TEQL|S:    expr->constant.i = l->constant.i == r->constant.i; break;
	case TEQL|F:    expr->constant.i = l->constant.f == r->constant.f; break;
	case TNEQ:
	case TNEQ|S:    expr->constant.i = l->constant.i != r->constant.i; break;
	case TNEQ|F:    expr->constant.i = l->constant.f != r->constant.f; break;
	default:
		fatal("internal error; unknown binary expression");
	}
	cast(expr);
}
#undef F
#undef S

struct expr *
eval(struct expr *expr)
{
	struct expr *l, *r, *c;
	struct decl *d;

	switch (expr->kind) {
	case EXPRIDENT:
		if (expr->ident.decl->kind != DECLCONST)
			break;
		expr->kind = EXPRCONST;
		expr->constant.i = intconstvalue(expr->ident.decl->value);
		break;
	case EXPRCOMPOUND:
		d = mkdecl(DECLOBJECT, expr->type, expr->qual, LINKNONE);
		d->value = mkglobal(NULL, true);
		emitdata(d, expr->compound.init);
		expr->kind = EXPRIDENT;
		expr->ident.decl = d;
		break;
	case EXPRUNARY:
		l = eval(expr->unary.base);
		if (expr->unary.op != TBAND)
			break;
		switch (l->kind) {
		case EXPRUNARY:
			if (l->unary.op == TMUL)
				expr = eval(l->unary.base);
			break;
		case EXPRSTRING:
			l->ident.decl = stringdecl(l);
			l->kind = EXPRIDENT;
			expr->unary.base = l;
			break;
		}
		break;
	case EXPRCAST:
		l = eval(expr->cast.e);
		if (l->kind == EXPRCONST) {
			expr->kind = EXPRCONST;
			if (l->type->prop & PROPINT && expr->type->prop & PROPFLOAT)
				expr->constant.f = l->constant.i;
			else if (l->type->prop & PROPFLOAT && expr->type->prop & PROPINT)
				expr->constant.i = l->constant.f;
			else
				expr->constant = l->constant;
			cast(expr);
		} else if (l->type->kind == TYPEPOINTER && expr->type->kind == TYPEPOINTER) {
			expr = l;
		}
		break;
	case EXPRBINARY:
		l = eval(expr->binary.l);
		r = eval(expr->binary.r);
		expr->binary.l = l;
		expr->binary.r = r;
		switch (expr->binary.op) {
		case TADD:
			if (r->kind == EXPRBINARY)
				c = l, l = r, r = c;
			if (r->kind != EXPRCONST)
				break;
			if (l->kind == EXPRCONST) {
				binary(expr, expr->binary.op, l, r);
			} else if (l->kind == EXPRBINARY && l->type->kind == TYPEPOINTER && l->binary.r->kind == EXPRCONST) {
				if (l->binary.op == TADD || l->binary.op == TSUB) {
					/* (P ± C1) + C2  ->  P + (C2 ± C1) */
					expr->binary.l = l->binary.l;
					binary(expr->binary.r, l->binary.op, r, l->binary.r);
				}
			}
			break;
		/* TODO: TSUB pointer handling */
		case TLOR:
			if (l->kind != EXPRCONST)
				break;
			return l->constant.i ? l : r;
		case TLAND:
			if (l->kind != EXPRCONST)
				break;
			return l->constant.i ? r : l;
		default:
			if (l->kind != EXPRCONST || r->kind != EXPRCONST)
				break;
			binary(expr, expr->binary.op, l, r);
		}
		break;
	case EXPRCOND:
		l = expr->cond.t;
		r = expr->cond.f;
		c = eval(expr->cond.e);
		if (c->kind != EXPRCONST)
			break;
		return eval(c->constant.i ? l : r);
	}

	return expr;
}
