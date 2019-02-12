#define DYNAMICLINKER "/lib64/ld-linux-x86-64.so.2"

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

		/* prevent libc from using GNU C extensions */
		"-U", "__GNUC__",

		/* required for glibc headers */
		"-D", "__restrict=restrict",
		"-D", "__extension__=",
		"-D", "__attribute__(x)=",
		"-D", "__asm__(x)=",
};
static char *compilecmd[] = {"cc-qbe"};
static char *codegencmd[] = {"qbe"};
static char *assemblecmd[] = {"as"};
static char *linkcmd[] = {"ld", "--dynamic-linker=" DYNAMICLINKER};
