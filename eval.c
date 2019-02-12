#include <stddef.h>
#include <stdint.h>
#include "util.h"
#include "decl.h"
#include "emit.h"
#include "eval.h"
#include "expr.h"
#include "token.h"
#include "type.h"

struct expression *
eval(struct expression *expr)
{
	struct expression *l, *r, *c;
	int op;

	switch (expr->kind) {
	case EXPRIDENT:
		if (expr->ident.decl->kind != DECLCONST)
			break;
		expr->kind = EXPRCONST;
		expr->constant.i = intconstvalue(expr->ident.decl->value);
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
			if (typeprop(l->type) & PROPINT && typeprop(expr->type) & PROPFLOAT)
				expr->constant.i = l->constant.f;
			else if (typeprop(l->type) & PROPFLOAT && typeprop(expr->type) & PROPINT)
				expr->constant.f = l->constant.i;
			else
				expr->constant = l->constant;
		} else if (l->type->kind == TYPEPOINTER && expr->type->kind == TYPEPOINTER) {
			expr = l;
		}
		break;
	case EXPRBINARY:
		l = eval(expr->binary.l);
		r = eval(expr->binary.r);
		expr->binary.l = l;
		expr->binary.r = r;
		if (l->kind != EXPRCONST)
			break;
		if (expr->binary.op == TLOR)
			return !l->constant.i ? r : l;
		if (expr->binary.op == TLAND)
			return l->constant.i ? r : l;
		if (r->kind != EXPRCONST)
			break;
		expr->kind = EXPRCONST;
		op = expr->binary.op;
#define F (1<<8)
#define S (2<<8)
		if (typeprop(l->type) & PROPFLOAT)
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
#undef F
#undef S
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
