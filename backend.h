struct gotolabel {
	struct value *label;
	_Bool defined;
};

struct switchcases {
	void *root;
	struct value *defaultlabel;
};

struct representation;
struct declaration;
struct expression;
struct initializer;
struct scope;
struct type;

struct switchcases *mkswitch(void);
void switchcase(struct switchcases *, uint64_t, struct value *);

struct value *mkblock(char *);
struct value *mkglobal(char *, _Bool);
struct value *mkintconst(struct representation *, uint64_t);

uint64_t intconstvalue(struct value *);

struct function *mkfunc(char *, struct type *, struct scope *);
struct type *functype(struct function *);
void funclabel(struct function *, struct value *);
struct value *funcexpr(struct function *, struct expression *);
void funcjmp(struct function *, struct value *);
void funcjnz(struct function *, struct value *, struct value *, struct value *);
void funcret(struct function *, struct value *);
struct gotolabel *funcgoto(struct function *, char *);
void funcswitch(struct function *, struct value *, struct switchcases *, struct value *);
void funcinit(struct function *, struct declaration *, struct initializer *);

void emitfunc(struct function *, _Bool);
void emitdata(struct declaration *,  struct initializer *);

extern struct representation i8, i16, i32, i64, f32, f64;
