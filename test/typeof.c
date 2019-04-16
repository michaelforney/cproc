int v;
typedef int* t;
int f() {v = 1; return 0;}
int main() {
  __typeof__(f()) x = 123;
  __typeof__(t) y = &x;

  if (x != 123)
    return 1;
  if (*y != 123)
    return 2;
  if (sizeof(y) != sizeof(int*))
    return 3;
  if (sizeof(x) != sizeof(int))
    return 4;
  if (v != 0)
    return 5;

  return 0;
}
