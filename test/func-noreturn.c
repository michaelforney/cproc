_Noreturn void exit(int);
int puts(const char *);
void _Noreturn f(void) {
	static int c;
	while (c)
		puts("loop");
}
int main(void) {
	exit(0);
	(*f)();
	(***f)();
	return 1;
}
