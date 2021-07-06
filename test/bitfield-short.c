struct {
	short x : 7;
} s = {.x = -64};

int main(void) {
	return s.x > 0;
}
