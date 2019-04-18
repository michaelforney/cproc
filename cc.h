struct func;

enum tokenkind {
	TNONE,

	TEOF,
	TNEWLINE,

	TIDENT,
	TNUMBER,
	TCHARCONST,
	TSTRINGLIT,

	/* keyword */
	TAUTO,
	TBREAK,
	TCASE,
	TCHAR,
	TCONST,
	TCONTINUE,
	TDEFAULT,
	TDO,
	TDOUBLE,
	TELSE,
	TENUM,
	TEXTERN,
	TFLOAT,
	TFOR,
	TGOTO,
	TIF,
	TINLINE,
	TINT,
	TLONG,
	TREGISTER,
	TRESTRICT,
	TRETURN,
	TSHORT,
	TSIGNED,
	TSIZEOF,
	TSTATIC,
	TSTRUCT,
	TSWITCH,
	TTYPEDEF,
	TUNION,
	TUNSIGNED,
	TVOID,
	TVOLATILE,
	TWHILE,
	T_ALIGNAS,
	T_ALIGNOF,
	T_ATOMIC,
	T_BOOL,
	T_COMPLEX,
	T_GENERIC,
	T_IMAGINARY,
	T_NORETURN,
	T_STATIC_ASSERT,
	T_THREAD_LOCAL,
	T__TYPEOF__,

	/* punctuator */
	TLBRACK,
	TRBRACK,
	TLPAREN,
	TRPAREN,
	TLBRACE,
	TRBRACE,
	TPERIOD,
	TARROW,
	TINC,
	TDEC,
	TBAND,
	TMUL,
	TADD,
	TSUB,
	TBNOT,
	TLNOT,
	TDIV,
	TMOD,
	TSHL,
	TSHR,
	TLESS,
	TGREATER,
	TLEQ,
	TGEQ,
	TEQL,
	TNEQ,
	TXOR,
	TBOR,
	TLAND,
	TLOR,
	TQUESTION,
	TCOLON,
	TSEMICOLON,
	TELLIPSIS,
	TASSIGN,
	TMULASSIGN,
	TDIVASSIGN,
	TMODASSIGN,
	TADDASSIGN,
	TSUBASSIGN,
	TSHLASSIGN,
	TSHRASSIGN,
	TBANDASSIGN,
	TXORASSIGN,
	TBORASSIGN,
	TCOMMA,
	THASH,
	THASHHASH,
};

struct location {
	const char *file;
	size_t line, col;
};

struct token {
	enum tokenkind kind;
	struct location loc;
	char *lit;
};

enum typequal {
	QUALNONE,

	QUALCONST    = 1<<1,
	QUALRESTRICT = 1<<2,
	QUALVOLATILE = 1<<3,
	QUALATOMIC   = 1<<4,
};

enum typekind {
	TYPENONE,

	TYPEVOID,
	TYPEBASIC,
	TYPEPOINTER,
	TYPEARRAY,
	TYPEFUNC,
	TYPESTRUCT,
	TYPEUNION,
};

enum typeprop {
	PROPNONE,

	PROPOBJECT  = 1<<0,
	PROPCHAR    = 1<<1,
	PROPINT     = 1<<2,
	PROPREAL    = 1<<3,
	PROPARITH   = 1<<4,
	PROPSCALAR  = 1<<5,
	PROPAGGR    = 1<<6,
	PROPDERIVED = 1<<7,
	PROPFLOAT   = 1<<8,
};

struct param {
	char *name;
	struct type *type;
	enum typequal qual;
	struct value *value;
	struct param *next;
};

struct bitfield {
	short before;  /* number of bits in the storage unit before the bit-field */
	short after;   /* number of bits in the storage unit after the bit-field */
};

struct member {
	char *name;
	struct type *type;
	enum typequal qual;
	uint64_t offset;
	struct bitfield bits;
	struct member *next;
};

struct type {
	enum typekind kind;
	int align;
	uint64_t size;
	struct repr *repr;
	union {
		struct type *base;
		struct list link;  /* used only during construction of type */
	};
	/* qualifiers of the base type */
	enum typequal qual;
	_Bool incomplete;
	union {
		struct {
			enum {
				BASICBOOL,
				BASICCHAR,
				BASICSHORT,
				BASICINT,
				BASICENUM,
				BASICLONG,
				BASICLONGLONG,
				BASICFLOAT,
				BASICDOUBLE,
				BASICLONGDOUBLE,
			} kind;
			_Bool issigned, iscomplex;
		} basic;
		struct {
			uint64_t length;
		} array;
		struct {
			_Bool isprototype, isvararg, isnoreturn, paraminfo;
			struct param *params;
		} func;
		struct {
			char *tag;
			struct member *members;
		} structunion;
	};
};

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
	BUILTINTYPESCOMPATIBLEP,
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
	enum typequal qual;
	struct value *value;

	/* objects and functions */
	struct list link;
	int align;  /* may be more strict than type requires */
	_Bool tentative, defined;

	/* built-ins */
	enum builtinkind builtin;
};

struct scope {
	struct map *tags;
	struct map *decls;
	struct value *breaklabel;
	struct value *continuelabel;
	struct switchcases *switchcases;
	struct scope *parent;
};

enum exprkind {
	/* primary expression */
	EXPRIDENT,
	EXPRCONST,
	EXPRSTRING,

	/* postfix expression */
	EXPRCALL,
	/* member E.M gets transformed to *(typeof(E.M) *)((char *)E + offsetof(typeof(E), M)) */
	EXPRBITFIELD,
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

struct expr {
	enum exprkind kind;
	/* whether this expression is an lvalue */
	_Bool lvalue;
	/* whether this expression is a pointer decayed from an array or function designator */
	_Bool decayed;
	/* the unqualified type of the expression */
	struct type *type;
	/* the type qualifiers of the object this expression refers to (ignored for non-lvalues) */
	enum typequal qual;
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
			struct expr *base;
			struct bitfield bits;
		} bitfield;
		struct {
			struct init *init;
		} compound;
		struct {
			enum tokenkind op;
			_Bool post;
			struct expr *base;
		} incdec;
		struct {
			enum tokenkind op;
			struct expr *base;
		} unary;
		struct {
			struct expr *e;
		} cast;
		struct {
			enum tokenkind op;
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

struct init {
	uint64_t start, end;
	struct expr *expr;
	struct bitfield bits;
	struct init *next;
};

/* token */

extern struct token tok;

void tokprint(const struct token *);
void tokdesc(char *, size_t, enum tokenkind, const char *);
_Noreturn void error(const struct location *, const char *, ...);

/* scan */

int scanfrom(const char *file);
void scan(struct token *);

void ppinit(const char *);

void next(void);
_Bool peek(int);
char *expect(int, const char *);
_Bool consume(int);

/* type */

struct type *mktype(enum typekind);
struct type *mkpointertype(struct type *, enum typequal);
struct type *mkarraytype(struct type *, enum typequal, uint64_t);

_Bool typecompatible(struct type *, struct type *);
_Bool typesame(struct type *, struct type *);
struct type *typecomposite(struct type *, struct type *);
struct type *typeunqual(struct type *, enum typequal *);
struct type *typecommonreal(struct type *, struct type *);
struct type *typeargpromote(struct type *);
struct type *typeintpromote(struct type *);
enum typeprop typeprop(struct type *);
struct member *typemember(struct type *, const char *, uint64_t *);

struct param *mkparam(char *, struct type *, enum typequal);

extern struct type typevoid;
extern struct type typebool;
extern struct type typechar, typeschar, typeuchar;
extern struct type typeshort, typeushort;
extern struct type typeint, typeuint;
extern struct type typelong, typeulong;
extern struct type typellong, typeullong;
extern struct type typefloat, typedouble, typelongdouble;
extern struct type typevalist, typevalistptr;

/* decl */

struct decl *mkdecl(enum declkind, struct type *, enum typequal, enum linkage);
_Bool decl(struct scope *, struct func *);
struct type *typename(struct scope *, enum typequal *);

struct decl *stringdecl(struct expr *);

void emittentativedefns(void);

/* scope */

void scopeinit(void);
struct scope *mkscope(struct scope *);
struct scope *delscope(struct scope *);

void scopeputdecl(struct scope *, const char *, struct decl *);
struct decl *scopegetdecl(struct scope *, const char *, _Bool);

void scopeputtag(struct scope *, const char *, struct type *);
struct type *scopegettag(struct scope *, const char *, _Bool);

extern struct scope filescope;

/* expr */

struct expr *expr(struct scope *);
struct expr *assignexpr(struct scope *);
struct expr *constexpr(struct scope *);
uint64_t intconstexpr(struct scope *, _Bool);
void delexpr(struct expr *);

struct expr *exprconvert(struct expr *, struct type *);

/* eval */

struct expr *eval(struct expr *);

/* init */

struct init *mkinit(uint64_t, uint64_t, struct bitfield, struct expr *);
struct init *parseinit(struct scope *, struct type *);

void stmt(struct func *, struct scope *);

/* backend */

struct gotolabel {
	struct value *label;
	_Bool defined;
};

struct switchcases {
	void *root;
	struct value *defaultlabel;
};

struct repr;

struct switchcases *mkswitch(void);
void switchcase(struct switchcases *, uint64_t, struct value *);

struct value *mkblock(char *);
struct value *mkglobal(char *, _Bool);
struct value *mkintconst(struct repr *, uint64_t);

uint64_t intconstvalue(struct value *);

struct func *mkfunc(char *, struct type *, struct scope *);
struct type *functype(struct func *);
void funclabel(struct func *, struct value *);
struct value *funcexpr(struct func *, struct expr *);
void funcjmp(struct func *, struct value *);
void funcjnz(struct func *, struct value *, struct value *, struct value *);
void funcret(struct func *, struct value *);
struct gotolabel *funcgoto(struct func *, char *);
void funcswitch(struct func *, struct value *, struct switchcases *, struct value *);
void funcinit(struct func *, struct decl *, struct init *);

void emitfunc(struct func *, _Bool);
void emitdata(struct decl *,  struct init *);

extern struct repr i8, i16, i32, i64, f32, f64;
