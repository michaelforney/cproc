struct {
	unsigned a : 2;
} s;

int main(void) {
	return -1 > s.a;
}
