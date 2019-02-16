enum typequalifier {
	QUALNONE,

	QUALCONST    = 1<<1,
	QUALRESTRICT = 1<<2,
	QUALVOLATILE = 1<<3,
	QUALATOMIC   = 1<<4,
};

enum typekind {
	TYPENONE,

	TYPEQUALIFIED,
	TYPEVOID,
	TYPEBASIC,
	TYPEPOINTER,
	TYPEARRAY,
	TYPEFUNC,
	TYPESTRUCT,
	TYPEUNION,
};

enum typeproperty {
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

struct parameter {
	char *name;
	struct type *type;
	struct value *value;
	struct parameter *next;
};

struct member {
	char *name;
	struct type *type;
	uint64_t offset;
};

struct type {
	enum typekind kind;
	int align;
	uint64_t size;
	struct representation *repr;
	union {
		struct type *base;
		struct list link;  /* used only during construction of type */
	};
	_Bool incomplete;
	union {
		struct {
			enum typequalifier kind;
		} qualified;
		struct {
			enum {
				BASICBOOL,
				BASICCHAR,
				BASICSHORT,
				BASICINT,
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
			_Bool isprototype, isvararg, isnoreturn;
			struct parameter *params;
		} func;
		struct {
			char *tag;
			struct array members;
		} structunion;
	};
};

struct type *mktype(enum typekind, struct type *base);
struct type *mkqualifiedtype(struct type *, enum typequalifier);
struct type *mkpointertype(struct type *);
struct type *mkarraytype(struct type *, uint64_t);

_Bool typecompatible(struct type *, struct type *);
_Bool typesame(struct type *, struct type *);
struct type *typecomposite(struct type *, struct type *);
struct type *typeunqual(struct type *, enum typequalifier *);
struct type *typecommonreal(struct type *, struct type *);
struct type *typeargpromote(struct type *);
struct type *typeintpromote(struct type *);
enum typeproperty typeprop(struct type *);
struct member *typemember(struct type *, const char *name);

struct parameter *mkparam(char *, struct type *);

extern struct type typevoid;
extern struct type typebool;
extern struct type typechar, typeschar, typeuchar;
extern struct type typeshort, typeushort;
extern struct type typeint, typeuint;
extern struct type typelong, typeulong;
extern struct type typellong, typeullong;
extern struct type typefloat, typedouble, typelongdouble;
extern struct type typevalist, typevalistptr;
