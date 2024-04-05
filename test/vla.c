short g() { return 1; }
long f(void) {
    char a[10 + g()];
    return sizeof(a);
}
