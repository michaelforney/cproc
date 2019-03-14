struct init {
	uint64_t start, end;
	struct expr *expr;
	struct init *next, *subinit;
};

struct scope;
struct type;

struct init *mkinit(uint64_t, uint64_t, struct expr *);
struct init *parseinit(struct scope *, struct type *);
