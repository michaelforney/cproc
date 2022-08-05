#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "cc.h"

struct macroparam {
	char *name;
	enum {
		PARAMTOK = 1<<0,  /* the parameter is used normally */
		PARAMSTR = 1<<1,  /* the parameter is used with the '#' operator */
		PARAMVAR = 1<<2,  /* the parameter is __VA_ARGS__ */
	} flags;
};

struct macroarg {
	struct token *token;
	size_t ntoken;
	/* stringized argument */
	struct token str;
};

struct macro {
	enum {
		MACROOBJ,
		MACROFUNC,
	} kind;
	char *name;
	/* whether or not this macro is ineligible for expansion */
	bool hide;
	/* parameters of function-like macro */
	struct macroparam *param;
	size_t nparam;
	/* argument tokens of macro invocation */
	struct macroarg *arg;
	/* replacement list */
	struct token *token;
	size_t ntoken;
};

struct frame {
	struct token *token;
	size_t ntoken;
	struct macro *macro;
};

enum ppflags ppflags;

static struct array ctx;
static struct map *macros;
/* number of macros currently undergoing expansion */
static size_t macrodepth;

void
ppinit(void)
{
	macros = mkmap(64);
	next();
}

/* check if two macro definitions are equal, as in C11 6.10.3p2 */
static bool
macroequal(struct macro *m1, struct macro *m2)
{
	struct macroparam *p1, *p2;
	struct token *t1, *t2;

	if (m1->kind != m2->kind)
		return false;
	if (m1->kind == MACROFUNC) {
		if (m1->nparam != m2->nparam)
			return false;
		for (p1 = m1->param, p2 = m2->param; p1 < m1->param + m1->nparam; ++p1, ++p2) {
			if (strcmp(p1->name, p2->name) != 0 || p1->flags != p2->flags)
				return false;
		}
	}
	if (m1->ntoken != m2->ntoken)
		return false;
	for (t1 = m1->token, t2 = m2->token; t1 < m1->token + m1->ntoken; ++t1, ++t2) {
		if (t1->kind != t2->kind)
			return false;
		if (t1->lit && strcmp(t1->lit, t2->lit) != 0)
			return false;
	}
	return true;
}

/* find the index of a macro parameter with the given name */
static size_t
macroparam(struct macro *m, struct token *t)
{
	size_t i;

	if (t->kind == TIDENT) {
		for (i = 0; i < m->nparam; ++i) {
			if (strcmp(m->param[i].name, t->lit) == 0)
				return i;
		}
	}
	return -1;
}

/* lookup a macro by name */
static struct macro *
macroget(char *name)
{
	struct mapkey k;

	mapkey(&k, name, strlen(name));
	return mapget(macros, &k);
}

static void
macrodone(struct macro *m)
{
	m->hide = false;
	if (m->kind == MACROFUNC && m->nparam > 0) {
		free(m->arg[0].token);
		free(m->arg);
	}
	--macrodepth;
}

static bool
macrovarargs(struct macro *m)
{
	return m->kind == MACROFUNC && m->nparam > 0 && m->param[m->nparam - 1].flags & PARAMVAR;
}

static struct token *
framenext(struct frame *f)
{
	return f->ntoken--, f->token++;
}

/* push a new context frame */
static struct frame *
ctxpush(struct token *t, size_t n, struct macro *m, bool space)
{
	struct frame *f;

	f = arrayadd(&ctx, sizeof(*f));
	f->token = t;
	f->ntoken = n;
	f->macro = m;
	if (n > 0)
		t[0].space = space;
	return f;
}

/* get the next token from the context */
static struct token *
ctxnext(void)
{
	struct frame *f;
	struct token *t;
	struct macro *m;
	bool space;
	size_t i;

again:
	for (f = arraylast(&ctx, sizeof(*f)); ctx.len; --f, ctx.len -= sizeof(*f)) {
		if (f->ntoken)
			break;
		if (f->macro)
			macrodone(f->macro);
	}
	if (ctx.len == 0)
		return NULL;
	m = f->macro;
	if (m && m->kind == MACROFUNC) {
		/* try to expand macro parameter */
		space = f->token->space;
		switch (f->token->kind) {
		case THASH:
			framenext(f);
			t = framenext(f);
			assert(t);
			i = macroparam(m, t);
			assert(i != -1);
			f = ctxpush(&m->arg[i].str, 1, NULL, space);
			break;
		case TIDENT:
			i = macroparam(m, f->token);
			if (i == -1)
				break;
			framenext(f);
			if (m->arg[i].ntoken == 0)
				goto again;
			f = ctxpush(m->arg[i].token, m->arg[i].ntoken, NULL, space);
			break;
		}
		/* XXX: token concatenation */
	}
	return framenext(f);
}

static void
define(void)
{
	struct token *t;
	enum tokenkind prev;
	struct macro *m;
	struct macroparam *p;
	struct array params = {0}, repl = {0};
	struct mapkey k;
	void **entry;
	size_t i;

	m = xmalloc(sizeof(*m));
	m->name = tokencheck(&tok, TIDENT, "after #define");
	m->hide = false;
	t = arrayadd(&repl, sizeof(*t));
	scan(t);
	if (t->kind == TLPAREN && !t->space) {
		m->kind = MACROFUNC;
		/* read macro parameter names */
		p = NULL;
		while (scan(&tok), tok.kind != TRPAREN) {
			if (p) {
				if (p->flags & PARAMVAR)
					tokencheck(&tok, TRPAREN, "after '...'");
				tokencheck(&tok, TCOMMA, "or ')' after macro parameter");
				scan(&tok);
			}
			p = arrayadd(&params, sizeof(*p));
			p->flags = 0;
			if (tok.kind == TELLIPSIS) {
				p->name = "__VA_ARGS__";
				p->flags |= PARAMVAR;
			} else {
				p->name = tokencheck(&tok, TIDENT, "of macro parameter name or '...'");
			}
		}
		scan(t);  /* first token in replacement list */
	} else {
		m->kind = MACROOBJ;
	}
	m->param = params.val;
	m->nparam = params.len / sizeof(m->param[0]);

	/* read macro body */
	i = macroparam(m, t);
	while (t->kind != TNEWLINE && t->kind != TEOF) {
		if (t->kind == THASHHASH)
			error(&t->loc, "'##' operator is not yet implemented");
		prev = t->kind;
		t = arrayadd(&repl, sizeof(*t));
		scan(t);
		if (t->kind == TIDENT && strcmp(t->lit, "__VA_ARGS__") == 0 && !macrovarargs(m))
			error(&t->loc, "__VA_ARGS__ can only be used in variadic function-like macros");
		if (m->kind != MACROFUNC)
			continue;
		if (i != -1)
			m->param[i].flags |= PARAMTOK;
		i = macroparam(m, t);
		if (prev == THASH) {
			tokencheck(t, TIDENT, "after '#' operator");
			if (i == -1)
				error(&t->loc, "'%s' is not a macro parameter name", t->lit);
			m->param[i].flags |= PARAMSTR;
			i = -1;
		}
	}
	m->token = repl.val;
	m->ntoken = repl.len / sizeof(*t) - 1;
	tok = *t;

	mapkey(&k, m->name, strlen(m->name));
	entry = mapput(macros, &k);
	if (*entry && !macroequal(m, *entry))
		error(&tok.loc, "redefinition of macro '%s'", m->name);
	*entry = m;
}

static void
undef(void)
{
	char *name;
	struct mapkey k;
	void **entry;
	struct macro *m;

	name = tokencheck(&tok, TIDENT, "after #undef");
	mapkey(&k, name, strlen(name));
	entry = mapput(macros, &k);
	m = *entry;
	if (m) {
		free(name);
		free(m->param);
		free(m->token);
		*entry = NULL;
	}
	scan(&tok);
}

static void
directive(void)
{
	struct location newloc;
	enum ppflags oldflags;
	char *name = NULL;

	scan(&tok);
	if (tok.kind == TNEWLINE)
		return;  /* empty directive */
	oldflags = ppflags;
	ppflags |= PPNEWLINE;
	if (tok.kind == TNUMBER)
		goto line;  /* gcc line markers */
	name = tokencheck(&tok, TIDENT, "newline, or number after '#'");
	if (strcmp(name, "if") == 0) {
		error(&tok.loc, "#if directive is not implemented");
	} else if (strcmp(name, "ifdef") == 0) {
		error(&tok.loc, "#ifdef directive is not implemented");
	} else if (strcmp(name, "ifndef") == 0) {
		error(&tok.loc, "#ifndef directive is not implemented");
	} else if (strcmp(name, "elif") == 0) {
		error(&tok.loc, "#elif directive is not implemented");
	} else if (strcmp(name, "endif") == 0) {
		error(&tok.loc, "#endif directive is not implemented");
	} else if (strcmp(name, "include") == 0) {
		error(&tok.loc, "#include directive is not implemented");
	} else if (strcmp(name, "define") == 0) {
		scan(&tok);
		define();
	} else if (strcmp(name, "undef") == 0) {
		scan(&tok);
		undef();
	} else if (strcmp(name, "line") == 0) {
		scan(&tok);
		tokencheck(&tok, TNUMBER, "after #line");
line:
		newloc.line = strtoull(tok.lit, NULL, 0);
		newloc.col = 1;
		scan(&tok);
		newloc.file = tok.loc.file;
		if (tok.kind == TSTRINGLIT) {
			/* XXX: handle escape sequences (reuse string decoding from expr.c) */
			newloc.file = strchr(tok.lit, '"') + 1;
			*strchr(newloc.file, '"') = '\0';
			scan(&tok);
		}
		while (tok.kind == TNUMBER)
			scan(&tok);
		scansetloc(newloc);
	} else if (strcmp(name, "error") == 0) {
		error(&tok.loc, "#error directive is not implemented");
	} else if (strcmp(name, "pragma") == 0) {
		while (tok.kind != TNEWLINE && tok.kind != TEOF)
			next();
	} else {
		error(&tok.loc, "invalid preprocessor directive #%s", name);
	}
	free(name);
	tokencheck(&tok, TNEWLINE, "after preprocessing directive");
	ppflags = oldflags;
}

/* get the next token without expanding it */
static void
nextinto(struct token *t)
{
	static bool newline = true;

	for (;;) {
		scan(t);
		if (newline && t->kind == THASH) {
			directive();
		} else {
			newline = tok.kind == TNEWLINE;
			break;
		}
	}
}

static struct token *
rawnext(void)
{
	struct token *t;

	t = ctxnext();
	if (!t) {
		t = &tok;
		nextinto(t);
	}
	return t;
}

static bool
peekparen(void)
{
	static struct array pending;
	struct token *t;
	struct frame *f;

	t = ctxnext();
	if (t) {
		if (t->kind == TLPAREN)
			return true;
		f = arraylast(&ctx, sizeof(*f));
		--f->token;
		++f->ntoken;
		return false;
	}
	pending.len = 0;
	do t = arrayadd(&pending, sizeof(*t)), nextinto(t);
	while (t->kind == TNEWLINE);
	if (t->kind == TLPAREN)
		return true;
	t = pending.val;
	ctxpush(t, pending.len / sizeof(*t), NULL, t[0].space);
	return false;
}

static void
stringize(struct array *buf, struct token *t)
{
	const char *lit;

	if ((t->space || t->kind == TNEWLINE) && buf->len > 1 && ((char *)buf->val)[buf->len - 1] != ' ')
		arrayaddbuf(buf, " ", 1);
	lit = t->lit ? t->lit : tokstr[t->kind];
	if (t->kind == TSTRINGLIT || t->kind == TCHARCONST) {
		for (; *lit; ++lit) {
			if (*lit == '\\' || *lit == '"')
				arrayaddbuf(buf, "\\", 1);
			arrayaddbuf(buf, lit, 1);
		}
	} else if (lit) {
		arrayaddbuf(buf, lit, strlen(lit));
	}
}

static void expandfunc(struct macro *);

static bool
expand(struct token *t)
{
	struct macro *m;
	bool space;

	if (t->kind != TIDENT)
		return false;
	m = macroget(t->lit);
	if (!m || m->hide)
		t->hide = true;
	if (t->hide)
		return false;
	space = t->space;
	if (m->kind == MACROFUNC) {
		if (!peekparen())
			return false;
		expandfunc(m);
	}
	ctxpush(m->token, m->ntoken, m, space);
	m->hide = true;
	++macrodepth;
	return true;
}

static void
expandfunc(struct macro *m)
{
	struct macroparam *p;
	struct macroarg *arg;
	struct array str, tok;
	size_t i, depth, paren;
	struct token *t;

	/* read macro arguments */
	paren = 0;
	depth = macrodepth;
	tok = (struct array){0};
	arg = xreallocarray(NULL, m->nparam, sizeof(*arg));
	t = rawnext();
	for (i = 0; i < m->nparam; ++i) {
		p = &m->param[i];
		if (p->flags & PARAMSTR) {
			str = (struct array){0};
			arrayaddbuf(&str, "\"", 1);
		}
		arg[i].ntoken = 0;
		for (;;) {
			if (t->kind == TEOF)
				error(&t->loc, "EOF when reading macro parameters");
			if (macrodepth <= depth) {
				/* adjust current macro depth, in case it got shallower */
				depth = macrodepth;
				if (paren == 0 && (t->kind == TRPAREN || t->kind == TCOMMA && !(p->flags & PARAMVAR)))
					break;
				switch (t->kind) {
				case TLPAREN: ++paren; break;
				case TRPAREN: --paren; break;
				}
				if (p->flags & PARAMSTR)
					stringize(&str, t);
			}
			if (p->flags & PARAMTOK && !expand(t)) {
				arrayaddbuf(&tok, t, sizeof(*t));
				++arg[i].ntoken;
			}
			t = rawnext();
		}
		if (p->flags & PARAMSTR) {
			arrayaddbuf(&str, "\"", 2);
			arg[i].str = (struct token){
				.kind = TSTRINGLIT,
				.lit = str.val,
			};
		}
		if (t->kind == TRPAREN)
			break;
		t = rawnext();
	}
	if (i + 1 < m->nparam)
		error(&t->loc, "not enough arguments for macro '%s'", m->name);
	if (t->kind != TRPAREN)
		error(&t->loc, "too many arguments for macro '%s'", m->name);
	for (i = 0, t = tok.val; i < m->nparam; ++i) {
		arg[i].token = t;
		t += arg[i].ntoken;
	}
	m->arg = arg;
}

static void
keyword(struct token *tok)
{
	static const struct {
		const char *name;
		int value;
	} keywords[] = {
		{"_Alignas",       TALIGNAS},
		{"_Alignof",       TALIGNOF},
		{"_Atomic",        T_ATOMIC},
		{"_Bool",          TBOOL},
		{"_Complex",       T_COMPLEX},
		{"_Decimal128",    T_DECIMAL128},
		{"_Decimal32",     T_DECIMAL32},
		{"_Decimal64",     T_DECIMAL64},
		{"_Generic",       T_GENERIC},
		{"_Imaginary",     T_IMAGINARY},
		{"_Noreturn",      T_NORETURN},
		{"_Static_assert", TSTATIC_ASSERT},
		{"_Thread_local",  TTHREAD_LOCAL},
		{"__alignof__",    TALIGNOF},
		{"__asm",          T__ASM__},
		{"__asm__",        T__ASM__},
		{"__attribute__",  T__ATTRIBUTE__},
		{"__inline",       TINLINE},
		{"__inline__",     TINLINE},
		{"__signed",       TSIGNED},
		{"__signed__",     TSIGNED},
		{"__thread",       TTHREAD_LOCAL},
		{"__typeof",       TTYPEOF},
		{"__typeof__",     TTYPEOF},
		{"__volatile__",   TVOLATILE},
		{"alignas",        TALIGNAS},
		{"alignof",        TALIGNOF},
		{"auto",           TAUTO},
		{"bool",           TBOOL},
		{"break",          TBREAK},
		{"case",           TCASE},
		{"char",           TCHAR},
		{"const",          TCONST},
		{"constexpr",      TCONSTEXPR},
		{"continue",       TCONTINUE},
		{"default",        TDEFAULT},
		{"do",             TDO},
		{"double",         TDOUBLE},
		{"else",           TELSE},
		{"enum",           TENUM},
		{"extern",         TEXTERN},
		{"false",          TFALSE},
		{"float",          TFLOAT},
		{"for",            TFOR},
		{"goto",           TGOTO},
		{"if",             TIF},
		{"inline",         TINLINE},
		{"int",            TINT},
		{"long",           TLONG},
		{"register",       TREGISTER},
		{"nullptr",        TNULLPTR},
		{"restrict",       TRESTRICT},
		{"return",         TRETURN},
		{"short",          TSHORT},
		{"signed",         TSIGNED},
		{"sizeof",         TSIZEOF},
		{"static",         TSTATIC},
		{"static_assert",  TSTATIC_ASSERT},
		{"struct",         TSTRUCT},
		{"switch",         TSWITCH},
		{"thread_local",   TTHREAD_LOCAL},
		{"true",           TTRUE},
		{"typedef",        TTYPEDEF},
		{"typeof",         TTYPEOF},
		{"typeof_unqual",  TTYPEOF_UNQUAL},
		{"union",          TUNION},
		{"unsigned",       TUNSIGNED},
		{"void",           TVOID},
		{"volatile",       TVOLATILE},
		{"while",          TWHILE},
	};
	size_t low = 0, high = LEN(keywords), mid;
	int cmp;

	while (low < high) {
		mid = (low + high) / 2;
		cmp = strcmp(tok->lit, keywords[mid].name);
		if (cmp == 0) {
			free(tok->lit);
			tok->kind = keywords[mid].value;
			tok->lit = NULL;
			break;
		}
		if (cmp < 0)
			high = mid;
		else
			low = mid + 1;
	}
}

void
next(void)
{
	struct token *t;

	do t = rawnext();
	while (expand(t) || t->kind == TNEWLINE && !(ppflags & PPNEWLINE));
	tok = *t;
	if (tok.kind == TIDENT)
		keyword(&tok);
}

bool
peek(int kind)
{
	static struct token pending;
	struct token old;

	old = tok;
	next();
	if (tok.kind == kind) {
		next();
		return true;
	}
	pending = tok;
	tok = old;
	ctxpush(&pending, 1, NULL, pending.space);
	return false;
}

char *
expect(enum tokenkind kind, const char *msg)
{
	char *lit;

	lit = tokencheck(&tok, kind, msg);
	next();

	return lit;
}

bool
consume(int kind)
{
	if (tok.kind != kind)
		return false;
	next();
	return true;
}
