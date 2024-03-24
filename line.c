#include <string.h>
#include <stdio.h>
int main(void) {
	extern int getmode(int);
	char line[256];
	fprintf(stderr, "mode 0=%d\n", getmode(0));
	fprintf(stderr, "mode 1=%d\n", getmode(1));
	fprintf(stderr, "mode 2=%d\n", getmode(2));
	puts("hello");
	FILE *f = fopen("b", "w");
	fprintf(stderr, "mode b=%d\n", getmode(fileno(f)));
	fputs("hello\n", f);
	fclose(f);
	f = fopen("c", "wt");
	fprintf(stderr, "mode c=%d\n", getmode(fileno(f)));
	fputs("hello\n", f);
	fclose(f);
	f = fopen("test/basic.c", "r");
	fgets(line, sizeof line, f);
	fprintf(stderr, "c=%d %d\n", line[strlen(line) - 2], line[strlen(line) - 1]);
	fclose(f);
}
