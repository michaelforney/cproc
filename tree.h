struct treenode {
	uint64_t key;
	void *child[2];
	int height;
	_Bool new;  /* set by treeinsert if this node was newly allocated */
};

void *treeinsert(void **, uint64_t, size_t);
