struct location;

void ppinit(const char *);

void next(void);
_Bool peek(int);
char *expect(int, const char *);
_Bool consume(int);
