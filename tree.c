#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "util.h"

#define MAXH (sizeof(void *) * 8 * 3 / 2)

static inline int
height(struct treenode *n) {
	return n ? n->height : 0;
}

static int
rot(void **p, struct treenode *x, int dir /* deeper side */)
{
	struct treenode *y = x->child[dir];
	struct treenode *z = y->child[!dir];
	int hx = x->height;
	int hz = height(z);
	if (hz > height(y->child[dir])) {
		/*
		 *   x
		 *  / \ dir          z
		 * A   y            / \
		 *    / \   -->    x   y
		 *   z   D        /|   |\
		 *  / \          A B   C D
		 * B   C
		 */
		x->child[dir] = z->child[!dir];
		y->child[!dir] = z->child[dir];
		z->child[!dir] = x;
		z->child[dir] = y;
		x->height = hz;
		y->height = hz;
		z->height = hz + 1;
	} else {
		/*
		 *   x               y
		 *  / \             / \
		 * A   y    -->    x   D
		 *    / \         / \
		 *   z   D       A   z
		 */
		x->child[dir] = z;
		y->child[!dir] = x;
		x->height = hz + 1;
		y->height = hz + 2;
		z = y;
	}
	*p = z;
	return z->height - hx;
}

/* balance *p, return 0 if height is unchanged.  */
static int balance(void **p)
{
	struct treenode *n = *p;
	int h0 = height(n->child[0]);
	int h1 = height(n->child[1]);
	if (h0 - h1 + 1u < 3u) {
		int old = n->height;
		n->height = h0 < h1 ? h1 + 1 : h0 + 1;
		return n->height - old;
	}
	return rot(p, n, h0<h1);
}

void *
treeinsert(void **root, unsigned long long key, size_t sz)
{
	void **a[MAXH];
	struct treenode *n = *root;
	int i = 0;

	a[i++] = root;
	while (n) {
		if (key == n->key) {
			n->new = false;
			return n;
		}
		a[i++] = &n->child[key > n->key];
		n = n->child[key > n->key];
	}
	assert(sz > sizeof(*n));
	n = xmalloc(sz);
	n->key = key;
	n->child[0] = n->child[1] = 0;
	n->height = 1;
	/* insert new node, rebalance ancestors.  */
	*a[--i] = n;
	while (i && balance(a[--i]))
		;
	n->new = true;
	return n;
}
