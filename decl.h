enum declkind {
	DECLTYPE,
	DECLOBJECT,
	DECLFUNC,
	DECLCONST,
	DECLBUILTIN,
};

enum linkage {
	LINKNONE,
	LINKINTERN,
	LINKEXTERN,
};

enum builtinkind {
	BUILTINALLOCA,
	BUILTINCONSTANTP,
	BUILTININFF,
	BUILTINNANF,
	BUILTINOFFSETOF,
	BUILTINVAARG,
	BUILTINVACOPY,
	BUILTINVAEND,
	BUILTINVALIST,
	BUILTINVASTART,
};

struct decl {
	enum declkind kind;
	enum linkage linkage;
	struct type *type;
	struct value *value;

	/* objects and functions */
	struct list link;
	int align;  /* may be more strict than type requires */
	_Bool tentative, defined;

	/* built-ins */
	enum builtinkind builtin;
};

struct scope;
struct func;

struct decl *mkdecl(enum declkind, struct type *, enum linkage);
_Bool decl(struct scope *, struct func *);
struct type *typename(struct scope *);

struct expr;
struct decl *stringdecl(struct expr *);

void emittentativedefns(void);
