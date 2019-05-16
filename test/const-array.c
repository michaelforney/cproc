/* C11 6.7.3p9 - type qualifiers on array type qualify the element type */
typedef int T[2];
void f(const T x) {
	x = 0;
}
