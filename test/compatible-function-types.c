void f1();
void f1(void);

void f2(int, char *);
void f2(int, char *);

void f3(int(const double));
void f3(int (*)(double));

void f4(const char *, ...);
void f5(const char[], ...);

void f6(const int);
void f6(int);

static_assert(!__builtin_types_compatible_p(void(), void(int)));
static_assert(!__builtin_types_compatible_p(void(int), void(char)));
static_assert(!__builtin_types_compatible_p(int(float), char(float)));
static_assert(!__builtin_types_compatible_p(void(int, ...), void(int)));
static_assert(!__builtin_types_compatible_p(void(const char *), void(char *)));
static_assert(!__builtin_types_compatible_p(void(char *), void(unsigned char *)));
static_assert(!__builtin_types_compatible_p(void(int, float), void(int, double)));
