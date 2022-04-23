#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "arg.h"
#include "cc.h"

static void
usage(void)
{
	fprintf(stderr, "usage: %s [input]\n", argv0);
	exit(2);
}

int
main(int argc, char *argv[])
{
	bool pponly = false;
	char *output = NULL, *target = NULL;

	argv0 = progname(argv[0], "cproc-qbe");
	ARGBEGIN {
	case 'E':
		pponly = true;
		break;
	case 't':
		target = EARGF(usage());
		break;
	case 'o':
		output = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND

	targinit(target);

	if (output && !freopen(output, "w", stdout))
		fatal("open %s:", output);

	if (argc) {
		while (argc--)
			scanfrom(argv[argc], NULL);
		scanopen();
	} else {
		scanfrom("<stdin>", stdin);
	}

	ppinit();
	if (pponly) {
		ppflags |= PPNEWLINE;
		while (tok.kind != TEOF) {
			tokenprint(&tok);
			next();
		}
	} else {
		scopeinit();
		while (tok.kind != TEOF) {
			if (!decl(&filescope, NULL)) {
				if (tok.kind == TSEMICOLON)
					error(&tok.loc, "unexpected ';' at top-level");
				error(&tok.loc, "expected declaration or function definition");
			}
		}
		emittentativedefns();
	}

	fflush(stdout);
	if (ferror(stdout))
		fatal("write failed");
	return 0;
}
