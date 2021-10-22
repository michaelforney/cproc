__asm__(
	".text\n"
	".global foo\n"
	"foo:\n"
	"ret\n"
);

int main(void) {
	return 0;
}