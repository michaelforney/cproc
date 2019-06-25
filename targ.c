#include <stdint.h>
#include <string.h>
#include "util.h"
#include "cc.h"

struct target *targ;

static struct target alltargs[] = {
	{
		.name = "x86_64",
		.typewchar = &typeint,
	},
	{
		.name = "aarch64",
		.typewchar = &typeuint,
	},
};

void
targinit(const char *name)
{
	size_t i;

	if (!name) {
		/* TODO: provide a way to set this default */
		targ = &alltargs[0];
		return;
	}
	for (i = 0; i < LEN(alltargs); ++i) {
		if (strcmp(alltargs[i].name, name) == 0) {
			targ = &alltargs[i];
			return;
		}
	}
	fatal("unknown target '%s'", name);
}
