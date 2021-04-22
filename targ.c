#include <stdint.h>
#include <string.h>
#include "util.h"
#include "cc.h"

struct target *targ;

static struct target alltargs[] = {
	{
		.name = "x86_64",
		.typewchar = &typeint,
		.signedchar = 1,
	},
	{
		.name = "aarch64",
		.typewchar = &typeuint,
	},
	{
		.name = "riscv64",
		.typewchar = &typeint,
	},
};

void
targinit(const char *name)
{
	size_t i;

	if (!name) {
		/* TODO: provide a way to set this default */
		targ = &alltargs[0];
	}
	for (i = 0; i < LEN(alltargs) && !targ; ++i) {
		if (strcmp(alltargs[i].name, name) == 0)
			targ = &alltargs[i];
	}
	if (!targ)
		fatal("unknown target '%s'", name);
	typechar.basic.issigned = targ->signedchar;
}
