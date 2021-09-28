int i;
float f;
void *p;
int main(void) {
	if (i ? 1 : 0)
		return 1;
	if (f ? 1 : 0)
		return 1;
	if (p ? 1 : 0)
		return 1;
}
