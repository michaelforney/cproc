int f(char *);
__typeof__(f) f, g;

__typeof__(g(0)) x;
int x;

typedef int *t;
const __typeof__(t) y;
int *const y;
