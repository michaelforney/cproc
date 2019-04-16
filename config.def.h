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

	/* we don't yet support these optional features */
	"-D", "__STDC_NO_ATOMICS__",
	"-D", "__STDC_NO_COMPLEX__",
	"-D", "__STDC_NO_VLA__",

	/* specify the GNU C extensions we support */
	"-U", "__GNUC__", "-D", "__GNUC__=1",
	"-U", "__GNUC_MINOR__", "-D", "__GNUC_MINOR__=0",

	/* prevent glibc from using statement expressions for assert */
	"-D", "__STRICT_ANSI__",

	/* ignore attributes */
	"-D", "__attribute__(x)=",

	/* alternate keywords */
	"-D", "__alignof__=_Alignof",
	"-D", "__inline=inline",
	"-D", "__signed__=signed",
};
static char *compilecmd[] = {"cc-qbe"};
static char *codegencmd[] = {"qbe"};
static char *assemblecmd[] = {"as"};
static char *linkcmd[] = {"ld", "--dynamic-linker=" DYNAMICLINKER};
