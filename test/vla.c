short g() { return 1; }
long f(void) {
    double a[10 + g()];
    return sizeof(a);
}
