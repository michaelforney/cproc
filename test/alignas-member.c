struct {
	int x;
	_Alignas(32) char s[32];
} s = {123, "abc"};
