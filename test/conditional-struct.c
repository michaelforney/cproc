struct s {
	int x;
};
int main(void) {
	struct s a = {1}, b = {2};
	return (a.x ? b : a).x != 2;
}
