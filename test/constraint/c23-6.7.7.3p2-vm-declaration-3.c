int x;
int main(void) {
	/* a has thread storage duration */
	static thread_local int (*a)[x = 1];
	return 0;
}
