int x = __builtin_types_compatible_p(unsigned, enum {A});
int y = __builtin_types_compatible_p(const int, int);  /* qualifiers are ignored */
int z = __builtin_types_compatible_p(int *, unsigned *);
