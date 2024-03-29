struct {
	int x;
	alignas(32) char s[32];
} s = {123, "abc"};
