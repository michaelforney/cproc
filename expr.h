enum expressionkind {
	/* primary expression */
	EXPRIDENT,
	EXPRCONST,
	EXPRSTRING,

	/* postfix expression */
	EXPRCALL,
	/* member E.M gets transformed to *(typeof(E.M) *)((char *)E + offsetof(typeof(E), M)) */
	EXPRINCDEC,
	EXPRCOMPOUND,
	/* subscript E1[E2] gets transformed to *((E1)+(E2)) */

	EXPRUNARY,
	EXPRCAST,
	EXPRBINARY,
	EXPRCOND,
	EXPRASSIGN,
	EXPRCOMMA,

	EXPRBUILTIN,
	EXPRTEMP,
};

enum expressionflags {
	EXPRFLAG_LVAL    = 1<<0,
	EXPRFLAG_DECAYED = 1<<1,
};

struct expression {
	enum expressionkind kind;
	enum expressionflags flags;
	struct type *type;
	struct expression *next;
	union {
		struct {
			struct declaration *decl;
		} ident;
		union {
			uint64_t i;
			double f;
		} constant;
		struct {
			char *data;
			size_t size;
		} string;
		struct {
			struct expression *func, *args;
			size_t nargs;
		} call;
		struct {
			struct initializer *init;
		} compound;
		struct {
			int op;
			_Bool post;
			struct expression *base;
		} incdec;
		struct {
			int op;
			struct expression *base;
		} unary;
		struct {
			struct expression *e;
		} cast;
		struct {
			int op;
			struct expression *l, *r;
		} binary;
		struct {
			struct expression *e, *t, *f;
		} cond;
		struct {
			struct expression *l, *r;
		} assign;
		struct {
			struct expression *exprs;
		} comma;
		struct {
			enum {
				BUILTINVASTART,
				BUILTINVAARG,
				BUILTINVAEND,
				BUILTINALLOCA,
			} kind;
			struct expression *arg;
		} builtin;
		struct value *temp;
	};
};

struct scope;

struct expression *expr(struct scope *);
struct expression *assignexpr(struct scope *);
uint64_t intconstexpr(struct scope *);
void delexpr(struct expression *);

void exprpromote(struct expression **);  // XXX: move to type
struct expression *exprconvert(struct expression *, struct type *);
