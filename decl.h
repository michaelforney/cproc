enum declarationkind {
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
	BUILTINVALIST,
	BUILTINVASTART,
	BUILTINVAARG,
	BUILTINVACOPY,
	BUILTINVAEND,
	BUILTINOFFSETOF,
	BUILTINALLOCA,
};

struct declaration {
	enum declarationkind kind;
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
struct function;

struct declaration *mkdecl(enum declarationkind, struct type *, enum linkage);
_Bool decl(struct scope *, struct function *);
struct type *typename(struct scope *);

struct expression;
struct declaration *stringdecl(struct expression *);

void emittentativedefns(void);
