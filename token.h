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
	T__BUILTIN_VA_LIST,

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

#if 0
enum tokenclass {
	/* declaration */
	STORCLASS    = 1<<1,
	TYPEQUAL     = 1<<2,
	TYPESPEC     = 1<<3,
	FUNCSPEC     = 1<<4,
	ALIGNSPEC    = 1<<5,
	STATICASSERT = 1<<6,
	DECLARATION  = (1<<7) - 1,
};
#endif

//struct token *tokdup(struct token *);
//enum tokenclass tokclass(struct token *);

extern struct token tok;

void tokprint(const struct token *);
_Noreturn void error(const struct location *, const char *, ...);
