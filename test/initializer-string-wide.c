char s[] = "aα€😀\xAA\xBBBB\xCCCCCCCC";
char u8[] = u8"aα€😀\xAA\xBBBB\xCCCCCCCC";
unsigned short u[] = u"aα€😀\xAA\xBBBB\xCCCCCCCC";
unsigned U[] = U"aα€😀\xAA\xBBBB\xCCCCCCCC";
__typeof__(L' ') L[] = L"aα€😀\xAA\xBBBB\xCCCCCCCC";

void f(void) {
	char s[] = "aα€😀\xAA\xBBBB\xCCCCCCCC";
	char u8[] = u8"aα€😀\xAA\xBBBB\xCCCCCCCC";
	unsigned short u[] = u"aα€😀\xAA\xBBBB\xCCCCCCCC";
	unsigned U[] = U"aα€😀\xAA\xBBBB\xCCCCCCCC";
	__typeof__(L' ') L[] = L"aα€😀\xAA\xBBBB\xCCCCCCCC";
}
