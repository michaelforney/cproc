int i;
float f;
void *p;
int main(void) {
	if (i || 0)
		return 1;
	if (f || 0)
		return 1;
	if (p || 0)
		return 1;
}
