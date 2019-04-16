int f(void);
int x = __builtin_constant_p(1+2*3);
int y = __builtin_constant_p(f());
