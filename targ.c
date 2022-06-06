#include <stdint.h>
#include <string.h>
#include "util.h"
#include "cc.h"

const struct target *targ;

static const struct target alltargs[] = {
	{
		.name = "x86_64-sysv",
		.typewchar = &typeint,
		.typevalist = &(struct type){
			.kind = TYPEARRAY,
			.align = 8, .size = 24,
			.u.array = {1}, .base = &(struct type){
				.kind = TYPESTRUCT,
				.align = 8, .size = 24,
			},
		},
		.signedchar = 1,
	},
	{
		.name = "aarch64",
		.typevalist = &(struct type){
			.kind = TYPESTRUCT,
			.align = 8, .size = 32,
			.u.structunion.tag = "va_list",
		},
		.typewchar = &typeuint,
	},
	{
		.name = "riscv64",
		.typevalist = &(struct type){
			.kind = TYPEPOINTER, .prop = PROPSCALAR,
			.align = 8, .size = 8,
			.base = &typevoid,
		},
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
	typechar.u.basic.issigned = targ->signedchar;
	typeadjvalist = typeadjust(targ->typevalist);
}
