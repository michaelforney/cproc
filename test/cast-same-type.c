struct S {int x;} s;
int main(void) {
	return ((struct S)s).x;
}
