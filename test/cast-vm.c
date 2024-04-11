int main(void) {
	int l = 0;
	(int (*)[++l])0;
	(int (*(*)(void))[++l])0;
	return l != 2;
}
