struct scope {
	struct hashtable *tags;
	struct hashtable *decls;
	struct value *breaklabel;
	struct value *continuelabel;
	struct switchcases *switchcases;
	struct scope *parent;
};

void scopeinit(void);
struct scope *mkscope(struct scope *);
struct scope *delscope(struct scope *);

struct decl;
void scopeputdecl(struct scope *, const char *, struct decl *);
struct decl *scopegetdecl(struct scope *, const char *, _Bool);

struct type;
void scopeputtag(struct scope *, const char *, struct type *);
struct type *scopegettag(struct scope *, const char *, _Bool);

extern struct scope filescope;
