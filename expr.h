enum exprkind {
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

enum exprflags {
	EXPRFLAG_LVAL    = 1<<0,
	EXPRFLAG_DECAYED = 1<<1,
};

struct expr {
	enum exprkind kind;
	enum exprflags flags;
	struct type *type;
	struct expr *next;
	union {
		struct {
			struct decl *decl;
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
			struct expr *func, *args;
			size_t nargs;
		} call;
		struct {
			struct init *init;
		} compound;
		struct {
			int op;
			_Bool post;
			struct expr *base;
		} incdec;
		struct {
			int op;
			struct expr *base;
		} unary;
		struct {
			struct expr *e;
		} cast;
		struct {
			int op;
			struct expr *l, *r;
		} binary;
		struct {
			struct expr *e, *t, *f;
		} cond;
		struct {
			struct expr *l, *r;
		} assign;
		struct {
			struct expr *exprs;
		} comma;
		struct {
			int kind;
			struct expr *arg;
		} builtin;
		struct value *temp;
	};
};

struct scope;

struct expr *expr(struct scope *);
struct expr *assignexpr(struct scope *);
uint64_t intconstexpr(struct scope *, _Bool);
void delexpr(struct expr *);

struct expr *exprconvert(struct expr *, struct type *);
