#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "decl.h"
#include "expr.h"
#include "init.h"
#include "pp.h"
#include "token.h"
#include "type.h"

struct object {
	uint64_t offset;
	struct type *type;
	union {
		struct member *mem;
		size_t idx;
	};
	bool iscur;
};

struct initparser {
	/* TODO: keep track of type depth, and allocate maximum possible
	   number of nested objects in initializer */
	struct object obj[32], *cur, *sub;
};

struct initializer *
mkinit(uint64_t start, uint64_t end, struct expression *expr)
{
	struct initializer *init;

	init = xmalloc(sizeof(*init));
	init->start = start;
	init->end = end;
	init->expr = expr;
	init->next = NULL;

	return init;
}

static void
initadd(struct initializer **init, struct initializer *new)
{
	struct initializer *next, *sub, *last;
	uint64_t offset;

	while (*init && new->start >= (*init)->end)
		init = &(*init)->next;
	next = *init;
	if (next && next->start <= new->start && new->end < next->end) {
		initadd(&next->subinit, new);
		last = NULL;  /* silence gcc, we know that next->subinit has at least one member */
		for (offset = next->start, sub = next->subinit; sub; offset = sub->end, last = sub, sub = sub->next) {
			if (sub->start != offset)
				return;
		}
		if (offset == next->end) {
			*init = next->subinit;
			last->next = next->next;
		}
	} else {
		*init = new;
		while (next && next->start < (*init)->end)
			next = next->next;
		(*init)->next = next;
	}
}

static void
updatearray(struct type *t, uint64_t i)
{
	if (!t->incomplete)
		return;
	if (++i > t->array.length) {
		t->array.length = i;
		t->size = i * t->base->size;
	}
}

static void
designator(struct scope *s, struct initparser *p)
{
	struct type *t;
	uint64_t offset;
	const char *name;

	p->sub = p->cur;
	t = p->sub->type;
	offset = p->sub->offset;
	for (;;) {
		switch (tok.kind) {
		case TLBRACK:
			if (t->kind != TYPEARRAY)
				error(&tok.loc, "index designator is only valid for array types");
			next();
			p->sub->idx = intconstexpr(s);
			if (t->incomplete)
				updatearray(t, p->sub->idx);
			else if (p->sub->idx >= t->array.length)
				error(&tok.loc, "index designator is larger than array length");
			expect(TRBRACK, "for index designator");
			t = t->base;
			offset += p->sub->idx * t->size;
			break;
		case TPERIOD:
			if (t->kind != TYPESTRUCT && t->kind != TYPEUNION)
				error(&tok.loc, "member designator only valid for struct/union types");
			next();
			name = expect(TIDENT, "for member designator");
			arrayforeach (&t->structunion.members, p->sub->mem) {
				if (strcmp(p->sub->mem->name, name) == 0)
					break;
			}
			if (!p->sub->mem)
				error(&tok.loc, "%s has no member named '%s'", t->kind == TYPEUNION ? "union" : "struct", name);
			t = p->sub->mem->type;
			offset += p->sub->mem->offset;
			break;
		default:
			expect(TASSIGN, "after designator");
			return;
		}
		if (++p->sub == p->obj + LEN(p->obj))
			fatal("internal error: too many designators");
		p->sub->type = t;
		p->sub->offset = offset;
		p->sub->iscur = false;
	}
}

static void
focus(struct initparser *p)
{
	struct type *t;
	uint64_t offset;

	offset = p->sub->offset;
	switch (p->sub->type->kind) {
	case TYPEARRAY:
		p->sub->idx = 0;
		if (p->sub->type->incomplete)
			updatearray(p->sub->type, p->sub->idx);
		t = p->sub->type->base;
		break;
	case TYPESTRUCT:
	case TYPEUNION:
		p->sub->mem = p->sub->type->structunion.members.val;
		t = p->sub->mem->type;
		break;
	default:
		t = p->sub->type;
	}
	if (++p->sub == p->obj + LEN(p->obj))
		fatal("internal error: too many designators");
	p->sub->type = typeunqual(t, NULL);
	p->sub->offset = offset;
	p->sub->iscur = false;
}

static void
advance(struct initparser *p)
{
	struct type *t;
	uint64_t offset;

	for (;;) {
		--p->sub;
		offset = p->sub->offset;
		switch (p->sub->type->kind) {
		case TYPEARRAY:
			++p->sub->idx;
			if (p->sub->type->incomplete)
				updatearray(p->sub->type, p->sub->idx);
			if (p->sub->idx < p->sub->type->array.length) {
				t = p->sub->type->base;
				offset += t->size * p->sub->idx;
				goto done;
			}
			break;
		case TYPESTRUCT:
			++p->sub->mem;
			if (p->sub->mem != (void *)((uintptr_t)p->sub->type->structunion.members.val + p->sub->type->structunion.members.len)) {
				t = p->sub->mem->type;
				offset += p->sub->mem->offset;
				goto done;
			}
			break;
		}
		if (p->sub == p->cur)
			error(&tok.loc, "too many initializers for type");
	}
done:
	++p->sub;
	p->sub->type = typeunqual(t, NULL);
	p->sub->offset = offset;
	p->sub->iscur = false;
}

/* 6.7.9 Initialization */
struct initializer *
parseinit(struct scope *s, struct type *t)
{
	struct initparser p;
	struct initializer *init = NULL;
	struct expression *expr;
	struct type *base;

	p.cur = NULL;
	p.sub = p.obj;
	p.sub->offset = 0;
	p.sub->type = typeunqual(t, NULL);
	p.sub->iscur = false;
	for (;;) {
		if (p.cur) {
			if (tok.kind == TLBRACK || tok.kind == TPERIOD)
				designator(s, &p);
			else if (p.sub != p.cur)
				advance(&p);
			else
				focus(&p);
		}
		if (tok.kind == TLBRACE) {
			next();
			if (p.cur && p.cur->type == p.sub->type)
				error(&tok.loc, "nested braces around scalar initializer");
			p.cur = p.sub;
			p.cur->iscur = true;
			continue;
		}
		expr = assignexpr(s);
		for (;;) {
			t = typeunqual(p.sub->type, NULL);
			switch (t->kind) {
			case TYPEARRAY:
				if (expr->flags & EXPRFLAG_DECAYED && expr->unary.base->kind == EXPRSTRING) {
					expr = expr->unary.base;
					base = typeunqual(t->base, NULL);
					if (!typecompatible(expr->type->base, base))
						error(&tok.loc, "array initializer is string literal with incompatible type");
					if (t->incomplete)
						updatearray(t, expr->string.size + 1);
					goto add;
				}
				break;
			case TYPESTRUCT:
			case TYPEUNION:
				if (typecompatible(expr->type, t))
					goto add;
				break;
			default:  /* scalar type */
				assert(typeprop(t) & PROPSCALAR);
				expr = exprconvert(expr, t);
				goto add;
			}
			focus(&p);
		}
	add:
		initadd(&init, mkinit(p.sub->offset, p.sub->offset + p.sub->type->size, expr));
		for (;;) {
			if (p.sub->type->kind == TYPEARRAY && p.sub->type->incomplete)
				p.sub->type->incomplete = false;
			if (!p.cur)
				return init;
			if (tok.kind == TCOMMA) {
				next();
				if (tok.kind != TRBRACE)
					break;
			} else if (tok.kind != TRBRACE) {
				error(&tok.loc, "expected ',' or '}' after initializer");
			}
			next();
			p.sub = p.cur;
			do p.cur = p.cur == p.obj ? NULL : p.cur - 1;
			while (p.cur && !p.cur->iscur);
		}
	}
}
