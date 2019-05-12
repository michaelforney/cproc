int main(void) {
	int x = 0, *p = 0 ? 0 : &(int){x};
	return *p;
}
