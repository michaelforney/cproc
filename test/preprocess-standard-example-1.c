/* C11 6.10.3.4p4 */
#define f(a) a*g
#define g(a) f(a)
f(2)(9)
