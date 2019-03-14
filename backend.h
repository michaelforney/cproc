struct gotolabel {
	struct value *label;
	_Bool defined;
};

struct switchcases {
	void *root;
	struct value *defaultlabel;
};

struct repr;
struct decl;
struct expr;
struct init;
struct scope;
struct type;

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
