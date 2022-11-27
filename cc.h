#include <stdio.h>

struct func;

enum tokenkind {
	TNONE,

	TEOF,
	TNEWLINE,
	TOTHER,

	TIDENT,
	TNUMBER,
	TCHARCONST,
	TSTRINGLIT,

	/* keyword */
	TALIGNAS,
	TALIGNOF,
	TAUTO,
	TBOOL,
	TBREAK,
	TCASE,
	TCHAR,
	TCONST,
	TCONSTEXPR,
	TCONTINUE,
	TDEFAULT,
	TDO,
	TDOUBLE,
	TELSE,
	TENUM,
	TEXTERN,
	TFALSE,
	TFLOAT,
	TFOR,
	TGOTO,
	TIF,
	TINLINE,
	TINT,
	TLONG,
	TNULLPTR,
	TREGISTER,
	TRESTRICT,
	TRETURN,
	TSHORT,
	TSIGNED,
	TSIZEOF,
	TSTATIC,
	TSTATIC_ASSERT,
	TSTRUCT,
	TSWITCH,
	TTHREAD_LOCAL,
	TTRUE,
	TTYPEDEF,
	TTYPEOF,
	TTYPEOF_UNQUAL,
	TUNION,
	TUNSIGNED,
	TVOID,
	TVOLATILE,
	TWHILE,
	T_ATOMIC,
	T_BITINT,
	T_COMPLEX,
	T_DECIMAL128,
	T_DECIMAL32,
	T_DECIMAL64,
	T_GENERIC,
	T_IMAGINARY,
	T_NORETURN,
	T__ASM__,
	T__ATTRIBUTE__,

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
	TCOLONCOLON,
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
	THASHHASH
};

struct location {
	const char *file;
	size_t line, col;
};

struct token {
	enum tokenkind kind;
	/* whether or not the token is ineligible for expansion */
	_Bool hide;
	/* whether or not the token was preceeded by a space */
	_Bool space;
	struct location loc;
	char *lit;
};

enum typequal {
	QUALNONE,

	QUALCONST    = 1<<1,
	QUALRESTRICT = 1<<2,
	QUALVOLATILE = 1<<3,
	QUALATOMIC   = 1<<4
};

enum typekind {
	TYPENONE,

	TYPEVOID,
	TYPEBOOL,
	TYPECHAR,
	TYPESHORT,
	TYPEINT,
	TYPEENUM,
	TYPELONG,
	TYPELLONG,
	TYPEFLOAT,
	TYPEDOUBLE,
	TYPELDOUBLE,
	TYPEPOINTER,
	TYPEARRAY,
	TYPEFUNC,
	TYPESTRUCT,
	TYPEUNION
};

enum typeprop {
	PROPNONE,

	PROPCHAR    = 1<<0,
	PROPINT     = 1<<1,
	PROPREAL    = 1<<2,
	PROPARITH   = 1<<3,
	PROPSCALAR  = 1<<4,
	PROPFLOAT   = 1<<5
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
	unsigned long long offset;
	struct bitfield bits;
	struct member *next;
};

struct type {
	enum typekind kind;
	enum typeprop prop;
	int align;
	unsigned long long size;
	struct value *value;  /* used by the backend */
	struct type *base;
	struct list link;  /* used only during construction of type */
	/* qualifiers of the base type */
	enum typequal qual;
	_Bool incomplete, flexible;
	union {
		struct {
			_Bool issigned, iscomplex;
		} basic;
		struct {
			unsigned long long length;
		} array;
		struct {
			_Bool isprototype, isvararg, isnoreturn, paraminfo;
			struct param *params;
			size_t nparam;
		} func;
		struct {
			char *tag;
			struct member *members;
		} structunion;
	} u;
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
	BUILTINEXPECT,
	BUILTININFF,
	BUILTINNANF,
	BUILTINOFFSETOF,
	BUILTINTYPESCOMPATIBLEP,
	BUILTINUNREACHABLE,
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
	char *asmname;
	_Bool defined;

	union {
		struct {
			/* link in list of tentative object definitions */
			struct list tentative;
			/* alignment of object storage (may be stricter than type requires) */
			int align;
		} obj;
		struct {
			/* the function might have an "inline definition" (C11 6.7.4p7) */
			_Bool inlinedefn;
		} func;
		enum builtinkind builtin;
	} u;
};

struct scope {
	struct map *tags;
	struct map *decls;
	struct block *breaklabel;
	struct block *continuelabel;
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

struct stringlit {
	size_t size;
	void *data;
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
	enum tokenkind op;
	struct expr *base;
	struct expr *next;
	union {
		struct {
			struct decl *decl;
		} ident;
		union {
			unsigned long long u;
			long long i;
			double f;
		} constant;
		struct stringlit string;
		struct {
			struct expr *args;
			size_t nargs;
		} call;
		struct {
			struct bitfield bits;
		} bitfield;
		struct {
			struct init *init;
		} compound;
		struct {
			_Bool post;
		} incdec;
		struct {
			struct expr *l, *r;
		} binary;
		struct {
			struct expr *t, *f;
		} cond;
		struct {
			struct expr *l, *r;
		} assign;
		struct {
			enum builtinkind kind;
		} builtin;
		struct value *temp;
	} u;
};

struct init {
	unsigned long long start, end;
	struct expr *expr;
	struct bitfield bits;
	struct init *next;
};

/* token */

extern struct token tok;
extern const char *tokstr[];

void tokenprint(const struct token *);
char *tokencheck(const struct token *, enum tokenkind, const char *);
void error(const struct location *, const char *, ...);

/* scan */

void scanfrom(const char *, FILE *);
void scanopen(void);
void scansetloc(struct location loc);
void scan(struct token *);

/* preprocessor */

enum ppflags {
	/* preserve newlines in preprocessor output */
	PPNEWLINE   = 1 << 0,
};

extern enum ppflags ppflags;

void ppinit(void);

void next(void);
_Bool peek(int);
char *expect(enum tokenkind, const char *);
_Bool consume(int);

/* type */

struct type *mktype(enum typekind, enum typeprop);
struct type *mkpointertype(struct type *, enum typequal);
struct type *mkarraytype(struct type *, enum typequal, unsigned long long);

_Bool typecompatible(struct type *, struct type *);
_Bool typesame(struct type *, struct type *);
struct type *typecomposite(struct type *, struct type *);
struct type *typeunqual(struct type *, enum typequal *);
struct type *typecommonreal(struct type *, unsigned, struct type *, unsigned);
struct type *typepromote(struct type *, unsigned);
struct type *typeadjust(struct type *);
enum typeprop typeprop(struct type *);
struct member *typemember(struct type *, const char *, unsigned long long *);

struct param *mkparam(char *, struct type *, enum typequal);

extern struct type typevoid;
extern struct type typebool;
extern struct type typechar, typeschar, typeuchar;
extern struct type typeshort, typeushort;
extern struct type typeint, typeuint;
extern struct type typelong, typeulong;
extern struct type typellong, typeullong;
extern struct type typefloat, typedouble, typeldouble;
extern struct type *typeadjvalist;

/* targ */

struct target {
	const char *name;
	struct type *typevalist;
	struct type *typewchar;
	int signedchar;
};

extern const struct target *targ;

void targinit(const char *);

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

struct type *stringconcat(struct stringlit *, _Bool);

struct expr *expr(struct scope *);
struct expr *assignexpr(struct scope *);
struct expr *constexpr(struct scope *);
unsigned long long intconstexpr(struct scope *, _Bool);
void delexpr(struct expr *);

struct expr *exprassign(struct expr *, struct type *);
struct expr *exprpromote(struct expr *);

/* eval */

enum evalkind {
	EVALARITH,  /* arithmetic constant expression */
	EVALINIT,   /* initializer constant expression */
};

struct expr *eval(struct expr *, enum evalkind);

/* init */

struct init *mkinit(unsigned long long, unsigned long long, struct bitfield, struct expr *);
struct init *parseinit(struct scope *, struct type *);

void stmt(struct func *, struct scope *);

/* backend */

struct gotolabel {
	struct block *label;
	_Bool defined;
};

struct switchcases {
	void *root;
	struct type *type;
	struct block *defaultlabel;
};

void switchcase(struct switchcases *, unsigned long long, struct block *);

struct block *mkblock(char *);

struct value *mkglobal(char *, _Bool);

struct value *mkintconst(unsigned long long);
unsigned long long intconstvalue(struct value *);

struct func *mkfunc(struct decl *, char *, struct type *, struct scope *);
void delfunc(struct func *);
struct type *functype(struct func *);
void funclabel(struct func *, struct block *);
struct value *funcexpr(struct func *, struct expr *);
void funcjmp(struct func *, struct block *);
void funcjnz(struct func *, struct value *, struct type *, struct block *, struct block *);
void funcret(struct func *, struct value *);
struct gotolabel *funcgoto(struct func *, char *);
void funcswitch(struct func *, struct value *, struct switchcases *, struct block *);
void funcinit(struct func *, struct decl *, struct init *);

void emitfunc(struct func *, _Bool);
void emitdata(struct decl *,  struct init *);
