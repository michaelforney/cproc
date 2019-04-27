typedef int x;

void f1(int(x));
void f1(int (*)(int));

void f2(int(y));
void f2(int);

void f3(int((*)));
void f3(int *);

void f4(int((*x)));
void f4(int *);

void f5(int((x)));
void f5(int (*)(int));
