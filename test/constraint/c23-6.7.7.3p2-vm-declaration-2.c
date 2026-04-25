int x;
int main(void) {
	/* a has static storage duration */
	static int (*a)[x = 1];
	return 0;
}
