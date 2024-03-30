#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

static unsigned long
hash(const void *ptr, size_t len)
{
	unsigned long h;
	const unsigned char *pos, *end;

	/* FNV-1a */
	h = 0x811c9dc5;
	for (pos = ptr, end = pos + len; pos != end; ++pos)
		h = (h ^ *pos) * 0x1000193;
	return h;
}

void
mapkey(struct mapkey *k, const void *s, size_t n)
{
	k->str = s;
	k->len = n;
	k->hash = hash(s, n);
}

void
mapinit(struct map *h, size_t cap)
{
	size_t i;

	assert(!(cap & cap - 1));
	h->len = 0;
	h->cap = cap;
	h->keys = xreallocarray(NULL, cap, sizeof(h->keys[0]));
	h->vals = xreallocarray(NULL, cap, sizeof(h->vals[0]));
	for (i = 0; i < cap; ++i)
		h->keys[i].str = NULL;
}

void
mapfree(struct map *h, void del(void *))
{
	size_t i;

	if (del) {
		for (i = 0; i < h->cap; ++i) {
			if (h->keys[i].str)
				del(h->vals[i]);
		}
	}
	free(h->keys);
	free(h->vals);
}

static bool
keyequal(struct mapkey *k1, struct mapkey *k2)
{
	if (k1->hash != k2->hash || k1->len != k2->len)
		return false;
	return memcmp(k1->str, k2->str, k1->len) == 0;
}

static size_t
keyindex(struct map *h, struct mapkey *k)
{
	size_t i;

	i = k->hash & h->cap - 1;
	while (h->keys[i].str && !keyequal(&h->keys[i], k))
		i = i + 1 & h->cap - 1;
	return i;
}

void **
mapput(struct map *h, struct mapkey *k)
{
	struct mapkey *oldkeys;
	void **oldvals;
	size_t i, j, oldcap;

	if (h->cap / 2 < h->len) {
		oldkeys = h->keys;
		oldvals = h->vals;
		oldcap = h->cap;
		h->cap *= 2;
		h->keys = xreallocarray(NULL, h->cap, sizeof(h->keys[0]));
		h->vals = xreallocarray(NULL, h->cap, sizeof(h->vals[0]));
		for (i = 0; i < h->cap; ++i)
			h->keys[i].str = NULL;
		for (i = 0; i < oldcap; ++i) {
			if (oldkeys[i].str) {
				j = keyindex(h, &oldkeys[i]);
				h->keys[j] = oldkeys[i];
				h->vals[j] = oldvals[i];
			}
		}
		free(oldkeys);
		free(oldvals);
	}
	i = keyindex(h, k);
	if (!h->keys[i].str) {
		h->keys[i] = *k;
		h->vals[i] = NULL;
		++h->len;
	}

	return &h->vals[i];
}

void *
mapget(struct map *h, struct mapkey *k)
{
	size_t i;

	i = keyindex(h, k);
	return h->keys[i].str ? h->vals[i] : NULL;
}
