struct {
	signed x : 4;
} s;
int main(void) {
	return (s.x = 15) != -1;
}
