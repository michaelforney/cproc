int f(c)
	unsigned char c;
{
	return c;
}

int main(void)
{
	return f(0x100);
}
