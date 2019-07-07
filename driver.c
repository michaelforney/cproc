#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

enum phaseid {
	PREPROCESS = 1,
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

struct input {
	char *name;
	enum phaseid phase;
	bool lib;
};

static struct {
	bool nostdlib;
	bool verbose;
} flags;
static struct phase phases[] = {
	[PREPROCESS] = {.name = "preprocess"},
	[COMPILE]    = {.name = "compile"},
	[CODEGEN]    = {.name = "codegen"},
	[ASSEMBLE]   = {.name = "assemble"},
	[LINK]       = {.name = "link"},
};

static void
usage(const char *fmt, ...)
{
	va_list ap;

	if (fmt) {
		fprintf(stderr, "%s: ", argv0);
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		fputc('\n', stderr);
	}
	fprintf(stderr, "usage: %s [-c|-S|-E] [-D name[=value]] [-U name] [-s] [-g] [-o output] input...\n", argv0);
	exit(2);
}

static enum phaseid
inputphase(const char *name)
{
	const char *dot;

	dot = strrchr(name, '.');
	if (dot) {
		++dot;
		if (strcmp(dot, "c") == 0)
			return PREPROCESS;
		if (strcmp(dot, "i") == 0)
			return COMPILE;
		if (strcmp(dot, "qbe") == 0)
			return CODEGEN;
		if (strcmp(dot, "s") == 0 || strcmp(dot, "S") == 0)
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
	if (slash)
		name = slash + 1;
	dot = strrchr(name, '.');
	baselen = dot ? (size_t)(--dot - name + 1) : strlen(name);
	result = xmalloc(baselen + strlen(ext) + 2);
	memcpy(result, name, baselen);
	result[baselen] = '.';
	strcpy(result + baselen + 1, ext);

	return result;
}

static int
spawn(pid_t *pid, struct array *args, posix_spawn_file_actions_t *actions)
{
	extern char **environ;
	char **arg;

	if (flags.verbose) {
		fprintf(stderr, "%s: spawning", argv0);
		for (arg = args->val; *arg; ++arg)
			fprintf(stderr, " %s", *arg);
		fputc('\n', stderr);
	}
	return posix_spawnp(pid, *(char **)args->val, actions, NULL, args->val, environ);
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
		if (pipe(pipefd) < 0) {
			ret = errno;
			goto err1;
		}
		if (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) < 0) {
			ret = errno;
			goto err2;
		}
		if (fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) < 0) {
			ret = errno;
			goto err2;
		}
		ret = posix_spawn_file_actions_adddup2(&actions, pipefd[1], 1);
		if (ret)
			goto err2;
	}

	ret = spawn(&phase->pid, &phase->cmd, &actions);
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

static void
buildobj(struct input *input, char *output, enum phaseid last)
{
	const char *phase, *ext;
	char *phaseoutput;
	size_t i, npids;
	pid_t pid;
	int status, ret, fd;
	enum phaseid first = input->phase;
	bool success = true;

	if (input->phase > last || input->phase == LINK)
		return;
	if (last == LINK) {
		last = ASSEMBLE;
		output = strdup("/tmp/cproc-XXXXXX");
		if (!output)
			fatal("strdup:");
		fd = mkstemp(output);
		if (fd < 0)
			fatal("mkstemp:");
		close(fd);
	} else if (output) {
		if (strcmp(output, "-") == 0)
			output = NULL;
	} else if (last != PREPROCESS) {
		switch (last) {
		case COMPILE:  ext = "qbe"; break;
		case CODEGEN:  ext = "s";   break;
		case ASSEMBLE: ext = "o";   break;
		}
		output = changeext(input->name, ext);
	}
	if (strcmp(input->name, "-") == 0)
		input->name = NULL;

	npids = 0;
	for (i = first, fd = -1, phaseoutput = NULL; i <= last; ++i, ++npids) {
		if (i == last)
			phaseoutput = output;
		ret = spawnphase(&phases[i], &fd, input->name, phaseoutput, i == last);
		if (ret) {
			warn("%s: spawn \"%s\": %s", phases[i].name, *(char **)phases[i].cmd.val, strerror(ret));
			goto kill;
		}
		input->name = phaseoutput;
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
}

static noreturn void
buildexe(struct input *inputs, size_t ninputs, char *output)
{
	struct phase *p = &phases[LINK];
	size_t i;
	int ret, status;
	pid_t pid;

	arrayaddptr(&p->cmd, "-o");
	arrayaddptr(&p->cmd, output);
	if (!flags.nostdlib && startfiles[0])
		arrayaddbuf(&p->cmd, startfiles, sizeof(startfiles));
	for (i = 0; i < ninputs; ++i) {
		if (inputs[i].lib)
			arrayaddptr(&p->cmd, "-l");
		arrayaddptr(&p->cmd, inputs[i].name);
	}
	if (!flags.nostdlib && endfiles[0])
		arrayaddbuf(&p->cmd, endfiles, sizeof(endfiles));
	arrayaddptr(&p->cmd, NULL);

	ret = spawn(&pid, &p->cmd, NULL);
	if (ret)
		fatal("%s: spawn \"%s\": %s", p->name, *(char **)p->cmd.val, strerror(errno));
	if (waitpid(pid, &status, 0) < 0)
		fatal("waitpid %ju:", (uintmax_t)pid);
	for (i = 0; i < ninputs; ++i) {
		if (inputs[i].phase < LINK)
			unlink(inputs[i].name);
	}
	exit(!succeeded(p->name, pid, status));
}

static char *
nextarg(char ***argv)
{
	if ((**argv)[2] != '\0')
		return &(**argv)[2];
	++*argv;
	if (!**argv)
		usage(NULL);
	return **argv;
}

static char *
compilecommand(char *arg)
{
	char self[PATH_MAX], *cmd;
	size_t n;

	n = readlink("/proc/self/exe", self, sizeof(self) - 5);
	if (n == -1) {
		n = strlen(arg);
		if (n > sizeof(self) - 5)
			fatal("argv[0] is too large");
		memcpy(self, arg, n);
	} else if (n == sizeof(self) - 5) {
		fatal("target of /proc/self/exe is too large");
	}
	strcpy(self + n, "-qbe");
	cmd = strdup(self);
	if (!cmd)
		fatal("strdup:");
	return cmd;
}

static int
hasprefix(const char *str, const char *pfx)
{
	return memcmp(str, pfx, strlen(pfx)) == 0;
}

int
main(int argc, char *argv[])
{
	enum phaseid first = 0, last = LINK;
	char *arg, *end, *output = NULL, *arch, *qbearch;
	struct array inputs = {0}, *cmd;
	struct input *input;
	size_t i;

	argv0 = progname(argv[0], "cproc");

	arrayaddbuf(&phases[PREPROCESS].cmd, preprocesscmd, sizeof(preprocesscmd));
	arrayaddptr(&phases[COMPILE].cmd, compilecommand(argv[0]));
	arrayaddbuf(&phases[CODEGEN].cmd, codegencmd, sizeof(codegencmd));
	arrayaddbuf(&phases[ASSEMBLE].cmd, assemblecmd, sizeof(assemblecmd));
	arrayaddbuf(&phases[LINK].cmd, linkcmd, sizeof(linkcmd));

	if (hasprefix(target, "x86_64-") || hasprefix(target, "amd64-")) {
		arch = "x86_64";
		qbearch = "amd64_sysv";
	} else if (hasprefix(target, "aarch64-")) {
		arch = "aarch64";
		qbearch = "arm64";
	} else {
		fatal("unsupported target '%s'", target);
	}
	arrayaddptr(&phases[COMPILE].cmd, "-t");
	arrayaddptr(&phases[COMPILE].cmd, arch);
	arrayaddptr(&phases[CODEGEN].cmd, "-t");
	arrayaddptr(&phases[CODEGEN].cmd, qbearch);

	for (;;) {
		++argv, --argc;
		arg = *argv;
		if (!arg)
			break;
		if (arg[0] != '-' || arg[1] == '\0') {
			input = arrayadd(&inputs, sizeof(*input));
			input->name = arg;
			input->lib = false;
			if (first)
				input->phase = first;
			else if (arg[1])
				input->phase = inputphase(arg);
			else
				usage("reading from standard input requires -x");
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
				usage(NULL);
			arrayaddptr(&phases[PREPROCESS].cmd, arg);
			arrayaddptr(&phases[PREPROCESS].cmd, *++argv);
		} else if (strcmp(arg, "-pipe") == 0) {
			/* ignore */
		} else if (strncmp(arg, "-std=", 5) == 0) {
			/* ignore */
		} else if (strcmp(arg, "-pedantic") == 0) {
			/* ignore */
		} else {
			if (arg[2] != '\0' && strchr("cESsv", arg[1]))
				usage(NULL);
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
				input = arrayadd(&inputs, sizeof(*input));
				input->name = nextarg(&argv);
				input->lib = true;
				input->phase = LINK;
				break;
			case 'M':
				if (strcmp(arg, "-M") == 0 || strcmp(arg, "-MM") == 0) {
					arrayaddptr(&phases[PREPROCESS].cmd, arg);
					last = PREPROCESS;
				} else if (strcmp(arg, "-MD") == 0 || strcmp(arg, "-MMD") == 0) {
					arrayaddptr(&phases[PREPROCESS].cmd, arg);
				} else if (strcmp(arg, "-MT") == 0 || strcmp(arg, "-MF") == 0) {
					if (!--argc)
						usage(NULL);
					arrayaddptr(&phases[PREPROCESS].cmd, arg);
					arrayaddptr(&phases[PREPROCESS].cmd, *++argv);
				} else {
					usage(NULL);
				}
				break;
			case 'O':
				/* ignore */
				break;
			case 'o':
				output = nextarg(&argv);
				break;
			case 'P':
				/* ignore */
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
			case 'v':
				flags.verbose = true;
				break;
			case 'W':
				if (arg[2] && arg[3] == ',') {
					switch (arg[2]) {
					case 'p': cmd = &phases[PREPROCESS].cmd; break;
					case 'a': cmd = &phases[ASSEMBLE].cmd; break;
					case 'l': cmd = &phases[LINK].cmd; break;
					default: usage(NULL);
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
			case 'x':
				arg = nextarg(&argv);
				if (strcmp(arg, "none") == 0)
					first = 0;
				else if (strcmp(arg, "c") == 0)
					first = PREPROCESS;
				else if (strcmp(arg, "cpp-output") == 0)
					first = COMPILE;
				else if (strcmp(arg, "qbe") == 0)
					first = CODEGEN;
				else if (strcmp(arg, "assembler") == 0)
					first = ASSEMBLE;
				else
					usage("unknown language '%s'", arg);
				break;
			default:
				usage(NULL);
			}
		}
	}

	for (i = 0; i < NPHASES; ++i)
		phases[i].cmdbase = phases[i].cmd.len;
	if (inputs.len == 0)
		usage(NULL);
	if (output) {
		if (strcmp(output, "-") == 0) {
			if (last >= ASSEMBLE)
				usage("cannot write object to stdout");
		} else if (last != LINK && inputs.len > sizeof(*input)) {
			usage("cannot specify -o with multiple input files without linking");
		}
	}
	arrayforeach (&inputs, input)
		buildobj(input, output, last);
	if (last == LINK) {
		if (!output)
			output = "a.out";
		buildexe(inputs.val, inputs.len / sizeof(*input), output);
	}
}
