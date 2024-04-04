typeof(nullptr) x = 0;
static_assert(sizeof x == sizeof(char *));
int *y = nullptr;
bool z = nullptr;
