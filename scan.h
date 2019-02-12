struct token;

struct scanner *mkscanner(const char *file);
void scan(struct scanner *, struct token *);
