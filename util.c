#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include "util.h"

char *argv0;

static void
vwarn(const char *fmt, va_list ap)
{
	fprintf(stderr, "%s: ", argv0);
	vfprintf(stderr, fmt, ap);
	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}
}

void
warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

noreturn void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
	exit(1);
}

void *
reallocarray(void *buf, size_t n, size_t m)
{
	if (n > 0 && SIZE_MAX / n < m) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(buf, n * m);
}

void *
xreallocarray(void *buf, size_t n, size_t m)
{
	buf = reallocarray(buf, n, m);
	if (!buf)
		fatal("reallocarray:");

	return buf;
}

void *
xmalloc(size_t len)
{
	void *buf;

	buf = malloc(len);
	if (!buf)
		fatal("malloc:");

	return buf;
}

char *
progname(char *name, char *fallback)
{
	char *slash;

	if (!name)
		return fallback;
	slash = strrchr(name, '/');
	return slash ? slash + 1 : name;
}

void *
arrayadd(struct array *a, size_t n)
{
	void *v;

	if (a->cap - a->len < n) {
		do a->cap = a->cap ? a->cap * 2 : 256;
		while (a->cap - a->len < n);
		a->val = realloc(a->val, a->cap);
		if (!a->val)
			fatal("realloc");
	}
	v = (char *)a->val + a->len;
	a->len += n;

	return v;
}

void
arrayaddptr(struct array *a, void *v)
{
	*(void **)arrayadd(a, sizeof(v)) = v;
}

void
arrayaddbuf(struct array *a, const void *src, size_t n)
{
	memcpy(arrayadd(a, n), src, n);
}

void
listinsert(struct list *list, struct list *new)
{
	new->next = list->next;
	new->prev = list;
	list->next->prev = new;
	list->next = new;
}

void
listinsertlist(struct list *list, struct list *new)
{
	if (new->next == new)
		return;
	new->next->prev = list;
	new->prev->next = list;
	list->next->prev = new->prev;
	list->next = new->next;
}

void
listremove(struct list *list)
{
	list->next->prev = list->prev;
	list->prev->next = list->next;
}
