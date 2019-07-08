void f1();
void f1(int, void *);

int f2();
int f2(double);

void f3() {}
void f3(void);

void f4(int, char *);
void f4(int, char *);

void f5(x, y) unsigned x; double y; {}
void f5(unsigned, double);

void f6(const char *, ...);
void f6(const char[], ...);

void f7(const int);
void f7(int);
