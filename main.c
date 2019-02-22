#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include "util.h"
#include "arg.h"
#include "decl.h"
#include "pp.h"
#include "scope.h"
#include "token.h"

static noreturn void
usage(void)
{
	fprintf(stderr, "usage: %s [input]\n", argv0);
	exit(2);
}

int
main(int argc, char *argv[])
{
	bool pponly = false;
	char *output = NULL;

	argv0 = progname(argv[0], "qbe");
	ARGBEGIN {
	case 'E':
		pponly = true;
		break;
	case 'o':
		output = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND

	if (argc == 1) {
		if (!freopen(argv[0], "r", stdin))
			fatal("open %s:", argv[0]);
	} else if (argc) {
		usage();
	}
	if (output) {
		if (!freopen(output, "w", stdout))
			fatal("open %s:", output);
	}

	ppinit(argc ? argv[0] : "<stdin>");
	if (pponly) {
		while (tok.kind != TEOF) {
			tokprint(&tok);
			next();
		}
	} else {
		scopeputdecl(&filescope, "__builtin_va_list", &builtinvalist);
		scopeputdecl(&filescope, "__builtin_va_start", &builtinvastart);
		scopeputdecl(&filescope, "__builtin_va_copy", &builtinvacopy);
		scopeputdecl(&filescope, "__builtin_va_arg", &builtinvaarg);
		scopeputdecl(&filescope, "__builtin_va_end", &builtinvaend);
		scopeputdecl(&filescope, "__builtin_offsetof", &builtinoffsetof);
		scopeputdecl(&filescope, "__builtin_alloca", &builtinalloca);
		while (tok.kind != TEOF) {
			if (!decl(&filescope, NULL))
				error(&tok.loc, "expected declaration or function definition");
		}
	}
	emittentativedefns();

	return 0;
}
