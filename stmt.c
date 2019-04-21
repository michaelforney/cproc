#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "cc.h"

static bool
gotolabel(struct func *f)
{
	char *name;
	struct gotolabel *g;

	if (tok.kind != TIDENT)
		return false;
	name = tok.lit;
	if (!peek(TCOLON))
		return false;
	g = funcgoto(f, name);
	g->defined = true;
	funclabel(f, g->label);
	return true;
}

/* 6.8 Statements and blocks */
void
stmt(struct func *f, struct scope *s)
{
	char *name;
	struct expr *e;
	struct type *t;
	struct value *v, *label[4];
	struct switchcases swtch = {0};
	uint64_t i;

	while (gotolabel(f))
		;
	switch (tok.kind) {
	/* 6.8.1 Labeled statements */
	case TCASE:
		next();
		if (!s->switchcases)
			error(&tok.loc, "'case' label must be in switch");
		label[0] = mkblock("switch_case");
		funclabel(f, label[0]);
		i = intconstexpr(s, true);
		switchcase(s->switchcases, i, label[0]);
		expect(TCOLON, "after case expression");
		stmt(f, s);
		break;
	case TDEFAULT:
		next();
		if (!s->switchcases)
			error(&tok.loc, "'default' label must be in switch");
		if (s->switchcases->defaultlabel)
			error(&tok.loc, "multiple 'default' labels");
		expect(TCOLON, "after 'default'");
		s->switchcases->defaultlabel = mkblock("switch_default");
		funclabel(f, s->switchcases->defaultlabel);
		stmt(f, s);
		break;

	/* 6.8.2 Compound statement */
	case TLBRACE:
		next();
		s = mkscope(s);
		while (tok.kind != TRBRACE) {
			if (gotolabel(f) || !decl(s, f))
				stmt(f, s);
		}
		s = delscope(s);
		next();
		break;

	/* 6.8.3 Expression statement */
	case TSEMICOLON:
		next();
		break;
	default:
		e = expr(s);
		v = funcexpr(f, e);
		delexpr(e);
		expect(TSEMICOLON, "after expression statement");
		break;

	/* 6.8.4 Selection statement */
	case TIF:
		next();
		s = mkscope(s);
		expect(TLPAREN, "after 'if'");
		e = exprconvert(expr(s), &typebool);
		v = funcexpr(f, e);
		delexpr(e);
		expect(TRPAREN, "after expression");

		label[0] = mkblock("if_true");
		label[1] = mkblock("if_false");
		funcjnz(f, v, label[0], label[1]);

		funclabel(f, label[0]);
		s = mkscope(s);
		stmt(f, s);
		s = delscope(s);

		if (consume(TELSE)) {
			label[2] = mkblock("if_join");
			funcjmp(f, label[2]);
			funclabel(f, label[1]);
			s = mkscope(s);
			stmt(f, s);
			s = delscope(s);
			funclabel(f, label[2]);
		} else {
			funclabel(f, label[1]);
		}
		s = delscope(s);
		break;
	case TSWITCH:
		next();

		s = mkscope(s);
		expect(TLPAREN, "after 'switch'");
		e = expr(s);
		expect(TRPAREN, "after expression");

		if (!(typeprop(e->type) & PROPINT))
			error(&tok.loc, "controlling expression of switch statement must have integer type");
		e = exprconvert(e, typeintpromote(e->type));

		label[0] = mkblock("switch_cond");
		label[1] = mkblock("switch_join");

		v = funcexpr(f, e);
		funcjmp(f, label[0]);
		s = mkscope(s);
		s->breaklabel = label[1];
		s->switchcases = &swtch;
		stmt(f, s);
		funcjmp(f, label[1]);

		funclabel(f, label[0]);
		funcswitch(f, v, &swtch, swtch.defaultlabel ? swtch.defaultlabel : label[1]);
		s = delscope(s);

		funclabel(f, label[1]);
		s = delscope(s);
		break;

	/* 6.8.5 Iteration statements */
	case TWHILE:
		next();
		s = mkscope(s);
		expect(TLPAREN, "after 'while'");
		e = expr(s);
		expect(TRPAREN, "after expression");

		label[0] = mkblock("while_cond");
		label[1] = mkblock("while_body");
		label[2] = mkblock("while_join");

		funclabel(f, label[0]);
		v = funcexpr(f, e);
		funcjnz(f, v, label[1], label[2]);
		funclabel(f, label[1]);
		s = mkscope(s);
		s->continuelabel = label[0];
		s->breaklabel = label[2];
		stmt(f, s);
		s = delscope(s);
		funcjmp(f, label[0]);
		funclabel(f, label[2]);
		s = delscope(s);
		break;
	case TDO:
		next();

		label[0] = mkblock("do_body");
		label[1] = mkblock("do_join");

		s = mkscope(s);
		s = mkscope(s);
		s->continuelabel = label[0];
		s->breaklabel = label[1];
		funclabel(f, label[0]);
		stmt(f, s);
		s = delscope(s);

		expect(TWHILE, "after 'do' statement");
		expect(TLPAREN, "after 'while'");
		e = expr(s);
		expect(TRPAREN, "after expression");

		v = funcexpr(f, e);
		funcjnz(f, v, label[0], label[1]);  // XXX: compare to 0
		funclabel(f, label[1]);
		s = delscope(s);
		expect(TSEMICOLON, "after 'do' statement");
		break;
	case TFOR:
		next();
		expect(TLPAREN, "after while");
		s = mkscope(s);
		if (!decl(s, f)) {
			if (tok.kind != TSEMICOLON) {
				e = expr(s);
				funcexpr(f, e);
				delexpr(e);
			}
			expect(TSEMICOLON, NULL);
		}

		label[0] = mkblock("for_cond");
		label[1] = mkblock("for_body");
		label[2] = mkblock("for_cont");
		label[3] = mkblock("for_join");

		funclabel(f, label[0]);
		if (tok.kind != TSEMICOLON) {
			e = expr(s);
			v = funcexpr(f, e);
			funcjnz(f, v, label[1], label[3]);
			delexpr(e);
		}
		expect(TSEMICOLON, NULL);
		e = tok.kind == TRPAREN ? NULL : expr(s);
		expect(TRPAREN, NULL);

		funclabel(f, label[1]);
		s = mkscope(s);
		s->breaklabel = label[3];
		s->continuelabel = label[2];
		stmt(f, s);
		s = delscope(s);

		funclabel(f, label[2]);
		if (e) {
			funcexpr(f, e);
			delexpr(e);
		}
		funcjmp(f, label[0]);
		funclabel(f, label[3]);
		s = delscope(s);
		break;

	/* 6.8.6 Jump statements */
	case TGOTO:
		next();
		name = expect(TIDENT, "after 'goto'");
		funcjmp(f, funcgoto(f, name)->label);
		expect(TSEMICOLON, "after 'goto' statement");
		break;
	case TCONTINUE:
		next();
		if (!s->continuelabel)
			error(&tok.loc, "'continue' statement must be in loop");
		funcjmp(f, s->continuelabel);
		expect(TSEMICOLON, "after 'continue' statement");
		break;
	case TBREAK:
		next();
		if (!s->breaklabel)
			error(&tok.loc, "'break' statement must be in loop or switch");
		funcjmp(f, s->breaklabel);
		expect(TSEMICOLON, "after 'break' statement");
		break;
	case TRETURN:
		next();
		t = functype(f);
		if (t->base != &typevoid) {
			e = exprconvert(expr(s), t->base);
			v = funcexpr(f, e);
			delexpr(e);
		} else {
			v = NULL;
		}
		funcret(f, v);
		expect(TSEMICOLON, "after 'return' statement");
		break;
	}
}
