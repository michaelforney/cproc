#define f(a, b) a ## b
#define x f(foo, bar)
#define y(x) f(x, bar)
#define foo abc
x
y(foo)
