struct mapkey {
	uint64_t hash;
	const char *str;
	size_t len;
};

void mapstrkey(struct mapkey *, const char *);
void mapbufkey(struct mapkey *, const char *, size_t);

struct map *mkmap(size_t);
void delmap(struct map *, void(void *));
void **mapput(struct map *, struct mapkey *);
void *mapget(struct map *, struct mapkey *);
