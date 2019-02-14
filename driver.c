#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

extern int pipe2(int[2], int);

enum phaseid {
	PREPROCESS,
	COMPILE,
	CODEGEN,
	ASSEMBLE,
	LINK,

	NPHASES,
};

#include "config.h"

struct phase {
	const char *name;
	struct array cmd;
	size_t cmdbase;
	pid_t pid;
};

extern char **environ;
static struct {
	bool nostdlib;
} flags;
static struct phase phases[] = {
	[PREPROCESS] = {.name = "preprocess"},
	[COMPILE]    = {.name = "compile"},
	[CODEGEN]    = {.name = "codegen"},
	[ASSEMBLE]   = {.name = "assemble"},
	[LINK]       = {.name = "link"},
};

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-c|-S|-E] [-D name[=value]] [-U name] [-s] [-g] [-o output] input...\n", argv0);
	exit(2);
}

static enum phaseid
inputphase(const char *name)
{
	const char *dot;

	dot = strrchr(name, '.');
	if (dot) {
		if (strcmp(dot, ".c") == 0)
			return PREPROCESS;
		if (strcmp(dot, ".i") == 0)
			return COMPILE;
		if (strcmp(dot, ".qbe") == 0)
			return CODEGEN;
		if (strcmp(dot, ".s") == 0 || strcmp(dot, ".S") == 0)
			return ASSEMBLE;
	}

	return LINK;
}

static char *
changeext(const char *name, const char *ext)
{
	const char *slash, *dot;
	char *result;
	size_t baselen;

	slash = strrchr(name, '/');
	if (!slash)
		slash = name;
	dot = strrchr(slash, '.');
	baselen = dot ? (size_t)(dot - name + 1) : strlen(name);
	result = xmalloc(baselen + strlen(ext) + 1);
	memcpy(result, name, baselen);
	strcpy(result + baselen, ext);

	return result;
}

static int
spawnphase(struct phase *phase, int *fd, char *input, char *output, bool last)
{
	int ret, pipefd[2];
	posix_spawn_file_actions_t actions;

	phase->cmd.len = phase->cmdbase;
	if (output) {
		arrayaddptr(&phase->cmd, "-o");
		arrayaddptr(&phase->cmd, output);
	}
	if (input)
		arrayaddptr(&phase->cmd, input);
	arrayaddptr(&phase->cmd, NULL);

	ret = posix_spawn_file_actions_init(&actions);
	if (ret)
		goto err0;
	if (*fd != -1)
		ret = posix_spawn_file_actions_adddup2(&actions, *fd, 0);
	if (!last) {
		if (pipe2(pipefd, O_CLOEXEC) < 0) {
			ret = errno;
			goto err1;
		}
		ret = posix_spawn_file_actions_adddup2(&actions, pipefd[1], 1);
		if (ret)
			goto err2;
	}

	ret = posix_spawnp(&phase->pid, *(char **)phase->cmd.val, &actions, NULL, phase->cmd.val, environ);
	if (ret)
		goto err2;
	if (!last) {
		*fd = pipefd[0];
		close(pipefd[1]);
	}
	posix_spawn_file_actions_destroy(&actions);

	return 0;

err2:
	if (!last) {
		close(pipefd[0]);
		close(pipefd[1]);
	}
err1:
	posix_spawn_file_actions_destroy(&actions);
err0:
	return ret;
}

static bool
succeeded(const char *phase, pid_t pid, int status)
{
	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) == 0)
			return true;
		warn("%s: process %ju exited with status %d", phase, (uintmax_t)pid, WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		warn("%s: process signaled: %s", phase, strsignal(WTERMSIG(status)));
	} else {
		warn("%s: process failed", phase);
	}
	return false;
}

static char *
buildobj(char *input, char *output, enum phaseid last)
{
	const char *phase;
	enum phaseid first;
	int status, ret, fd;
	bool success = true;
	size_t i, npids;
	pid_t pid;

	npids = 0;
	first = inputphase(input);
	if (first > last || first == LINK)
		return input;
	switch (last) {
	case COMPILE:
		if (!output)
			output = changeext(input, "qbe");
		break;
	case CODEGEN:
		if (!output)
			output = changeext(input, "s");
		break;
	case ASSEMBLE:
		if (!output)
			output = changeext(input, "o");
		break;
	case LINK:
		/* TODO: temporary object, or just removed later? */
		output = changeext(input, "o");
		last = ASSEMBLE;
		break;
	}
	if (output && strcmp(output, "-") == 0)
		output = NULL;

	for (i = first, fd = -1; i <= last; ++i, ++npids) {
		ret = spawnphase(&phases[i], &fd, i == first ? input : NULL, i == last ? output : NULL, i == last);
		if (ret) {
			warn("%s: spawn \"%s\": %s", phases[i].name, *(char **)phases[i].cmd.val, strerror(ret));
			goto kill;
		}
	}

	while (npids > 0) {
		pid = wait(&status);
		if (pid < 0)
			fatal("waitpid:");
		for (i = 0; i < NPHASES; ++i) {
			if (pid == phases[i].pid) {
				--npids;
				phases[i].pid = 0;
				phase = phases[i].name;
				break;
			}
		}
		if (i == NPHASES)
			continue;  /* unknown process */
		if (!succeeded(phase, pid, status)) {
kill:
			if (success && npids > 0) {
				for (i = 0; i < NPHASES; ++i) {
					if (phases[i].pid)
						kill(phases[i].pid, SIGTERM);
				}
			}
			success = false;
		}
	}
	if (!success) {
		if (output)
			unlink(output);
		exit(1);
	}
	return output;
}

static _Noreturn void
buildexe(char *inputs[], size_t ninputs, char *output)
{
	struct phase *p = &phases[LINK];
	size_t i;
	int ret, status;
	pid_t pid;

	arrayaddptr(&p->cmd, "-o");
	arrayaddptr(&p->cmd, output);
	if (!flags.nostdlib)
		arrayaddbuf(&p->cmd, startfiles, sizeof(startfiles));
	for (i = 0; i < ninputs; ++i)
		arrayaddptr(&p->cmd, inputs[i]);
	if (!flags.nostdlib)
		arrayaddbuf(&p->cmd, endfiles, sizeof(endfiles));
	arrayaddptr(&p->cmd, NULL);

	ret = posix_spawnp(&pid, *(char **)p->cmd.val, NULL, NULL, p->cmd.val, environ);
	if (ret)
		fatal("%s: spawn \"%s\": %s", p->name, *(char **)p->cmd.val, strerror(errno));
	if (waitpid(pid, &status, 0) < 0)
		fatal("waitpid %ju:", (uintmax_t)pid);
	exit(!succeeded(p->name, pid, status));
}

static char *
nextarg(char ***argv)
{
	if ((**argv)[2] != '\0')
		return &(**argv)[2];
	++*argv;
	if (!**argv)
		usage();
	return **argv;
}

static char *
compilecommand(void)
{
	char self[PATH_MAX], *cmd;
	ssize_t n;

	n = readlink("/proc/self/exe", self, sizeof(self) - 4);
	if (n < 0)
		fatal("readlink /proc/self/exe:");
	if (n == sizeof(self) - 4)
		fatal("target of /proc/self/exe is too large");
	strcpy(self + n, "-qbe");
	if (access(self, X_OK) < 0)
		return NULL;
	cmd = strdup(self);
	if (!cmd)
		fatal("strdup:");
	return cmd;
}

int
main(int argc, char *argv[])
{
	enum phaseid last = LINK;
	char *arg, *end, *output = NULL, **input;
	struct array inputs = {0}, *cmd;
	size_t i;

	arrayaddbuf(&phases[PREPROCESS].cmd, preprocesscmd, sizeof(preprocesscmd));
	arrayaddbuf(&phases[COMPILE].cmd, compilecmd, sizeof(compilecmd));
	arrayaddbuf(&phases[CODEGEN].cmd, codegencmd, sizeof(codegencmd));
	arrayaddbuf(&phases[ASSEMBLE].cmd, assemblecmd, sizeof(assemblecmd));
	arrayaddbuf(&phases[LINK].cmd, linkcmd, sizeof(linkcmd));
	arg = compilecommand();
	if (arg)
		*(char **)phases[COMPILE].cmd.val = arg;

	argv0 = progname(argv[0], "cc");
	for (;;) {
		++argv, --argc;
		arg = *argv;
		if (!arg)
			break;
		if (arg[0] != '-') {
			arrayaddptr(&inputs, arg);
			continue;
		}
		/* TODO: use a binary search for these long parameters */
		if (strcmp(arg, "-nostdlib") == 0) {
			flags.nostdlib = true;
		} else if (strcmp(arg, "-static") == 0) {
			arrayaddptr(&phases[LINK].cmd, arg);
		} else if (strcmp(arg, "-emit-qbe") == 0) {
			last = COMPILE;
		} else if (strcmp(arg, "-include") == 0 || strcmp(arg, "-idirafter") == 0) {
			if (!--argc)
				usage();
			arrayaddptr(&phases[PREPROCESS].cmd, arg);
			arrayaddptr(&phases[PREPROCESS].cmd, *++argv);
		} else if (strcmp(arg, "-pipe") == 0) {
			/* ignore */
		} else if (strncmp(arg, "-std=", 5) == 0) {
			/* ignore */
		} else if (strcmp(arg, "-pedantic") == 0) {
			/* ignore */
		} else {
			if (arg[2] != '\0' && strchr("cESs", arg[1]))
				usage();
			switch (arg[1]) {
			case 'c':
				last = ASSEMBLE;
				break;
			case 'D':
				arrayaddptr(&phases[PREPROCESS].cmd, "-D");
				arrayaddptr(&phases[PREPROCESS].cmd, nextarg(&argv));
				break;
			case 'E':
				last = PREPROCESS;
				break;
			case 'g':
				/* ignore */
				break;
			case 'I':
				arrayaddptr(&phases[PREPROCESS].cmd, "-I");
				arrayaddptr(&phases[PREPROCESS].cmd, nextarg(&argv));
				break;
			case 'L':
				arrayaddptr(&phases[LINK].cmd, "-L");
				arrayaddptr(&phases[LINK].cmd, nextarg(&argv));
				break;
			case 'l':
				arrayaddptr(&inputs, "-l");
				arrayaddptr(&inputs, nextarg(&argv));
				break;
			case 'O':
				/* ignore */
				break;
			case 'o':
				output = nextarg(&argv);
				break;
			case 'S':
				last = CODEGEN;
				break;
			case 's':
				arrayaddptr(&phases[LINK].cmd, "-s");
				break;
			case 'U':
				arrayaddptr(&phases[PREPROCESS].cmd, "-U");
				arrayaddptr(&phases[PREPROCESS].cmd, nextarg(&argv));
				break;
			case 'W':
				if (arg[2] && arg[3] == ',') {
					switch (arg[2]) {
					case 'p': cmd = &phases[PREPROCESS].cmd; break;
					case 'a': cmd = &phases[ASSEMBLE].cmd; break;
					case 'l': cmd = &phases[LINK].cmd; break;
					default: usage();
					}
					for (arg += 4; arg; arg = end ? end + 1 : NULL) {
						end = strchr(arg, ',');
						if (end)
							*end = '\0';
						arrayaddptr(cmd, arg);
					}
				} else {
					/* ignore warning flag */
				}
				break;
			default:
				usage();
			}
		}
	}

	for (i = 0; i < NPHASES; ++i)
		phases[i].cmdbase = phases[i].cmd.len;
	if (inputs.len == 0)
		usage();
	if (output && inputs.len / sizeof(char *) > 1 && last != LINK)
		fatal("cannot specify -o with multiple input files without linking");
	for (input = inputs.val; input < (char **)((char *)inputs.val + inputs.len); ++input) {
		if (strcmp(*input, "-l") == 0)
			++input;
		else
			*input = buildobj(*input, output, last);
	}
	if (last == LINK) {
		if (!output)
			output = "a.out";
		buildexe(inputs.val, inputs.len / sizeof(char *), output);
	}

	return 0;
}
