struct initializer {
	uint64_t start, end;
	struct expression *expr;
	struct initializer *next, *subinit;
};

struct scope;
struct type;
struct function;
struct declaration;

struct initializer *mkinit(uint64_t, uint64_t, struct expression *);
struct initializer *parseinit(struct scope *, struct type *);
