#define f(a, ...) __VA_ARGS__ + a, # __VA_ARGS__
f(abc, 1, (2, 3), 4)
