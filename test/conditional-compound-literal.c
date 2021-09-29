int main(void) {
	int x = 0, *p = x ? 0 : &(int){x};
	return *p;
}
