#ifndef DYNAMICLINKER
#define DYNAMICLINKER "/lib64/ld-linux-x86-64.so.2"
#endif

static char *startfiles[] = {
	"-l", ":crt1.o",
	"-l", ":crti.o",
};
static char *endfiles[] = {
	"-l", ":crtn.o",
	"-l", "c",
};

static char *preprocesscmd[] = {
		"cpp", "-P",

		/* specify the GNU C extensions we support */
		"-U", "__GNUC__", "-D", "__GNUC__=4",

		/* prevent glibc from using statement expressions for assert */
		"-D", "__STRICT_ANSI__",

		/* required for glibc headers */
		"-D", "__restrict=restrict",
		"-D", "__extension__=",
		"-D", "__attribute__(x)=",
		"-D", "__asm__(x)=",

		/* required for kernel headers */
		"-D", "__signed__=signed",
};
static char *compilecmd[] = {"cc-qbe"};
static char *codegencmd[] = {"qbe"};
static char *assemblecmd[] = {"as"};
static char *linkcmd[] = {"ld", "--dynamic-linker=" DYNAMICLINKER};
