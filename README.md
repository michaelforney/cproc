[![builds.sr.ht status](https://builds.sr.ht/~mcf.svg?search=~mcf/cc)](https://builds.sr.ht/~mcf?search=~mcf/cc)

This is a C11 compiler using [QBE] as a backend.

There is still much to do and the code is a little rough, but it currently
implements most of the language and is self-hosting.

Still TODO:

- Come up with a name so it can be installed alongside the system `cc`.
- The preprocessor (currently we are just using the system `cpp`).
- Bit-fields.
- Variable-length arrays.
- `volatile`-qualified types (requires support from QBE).
- `_Thread_local` storage-class specifier (requires support from QBE).
- `long double` type (requires support from QBE).
- Inline assembly (requires support from QBE).

[QBE]: https://c9x.me/compile/
