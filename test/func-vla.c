int f(int n, long long (*a)[n]) {
	return sizeof *a;
}

int main(void) {
	long long a[5];
	return f(5, &a) != sizeof a;
}
