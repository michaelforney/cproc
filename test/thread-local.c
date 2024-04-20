thread_local int a = 1;
static thread_local int b = 2;
extern thread_local int c = 3;
thread_local int d;
static thread_local int e;
extern thread_local int f;
int main(void) {
	static thread_local int x = 6;
	return a + b + c + d + e - x;
}
