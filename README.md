[![builds.sr.ht status](https://builds.sr.ht/~mcf/cc.svg)](https://builds.sr.ht/~mcf/cc)

This is a C11 compiler using [QBE] as a backend.

There is still much to do and the code is a little rough, but it currently
implements most of the language, is self-hosting, and capable of building
some useful software.

# Requirements

The compiler itself is written in standard [C11] and can be built with
any conforming C11 compiler.

The POSIX driver depends on POSIX.1-2008 interfaces, and the Makefile
requires a POSIX-compatible make(1).

The preprocessor is not yet implemented, so an existing one is currently
required.

# Target configuration

You will need to create a `config.h` appropriate for the target system. If
missing, a default version will be created from `config.def.h`,
which should work for most glibc systems, or musl systems with
`-D 'DYNAMIC_LINKER="/lib/ld-musl-x86_64.so.1"'`.

# What's missing

- Digraph and trigraph sequences ([6.4.6p3] and [5.2.1.1], will not
  be implemented).
- Wide string literals and character constants ([#35] and [#36]).
- Variable-length arrays ([#1]).
- `volatile`-qualified types ([#7]).
- `_Thread_local` storage-class specifier ([#5]).
- `long double` type ([#3]).
- Inline assembly ([#5]).
- Preprocessor ([#6]).
- Generation of position independent code (i.e. shared libraries,
  modules, PIEs).

# Issue tracker

Please report any issues to https://todo.sr.ht/~mcf/cc-issues.

[QBE]: https://c9x.me/compile/
[C11]: http://port70.net/~nsz/c/c11/n1570.html
[5.2.1.1]: http://port70.net/~nsz/c/c11/n1570.html#5.2.1.1
[6.4.6p3]: http://port70.net/~nsz/c/c11/n1570.html#6.4.6p3
[#1]: https://todo.sr.ht/~mcf/cc-issues/1
[#3]: https://todo.sr.ht/~mcf/cc-issues/3
[#5]: https://todo.sr.ht/~mcf/cc-issues/5
[#6]: https://todo.sr.ht/~mcf/cc-issues/6
[#7]: https://todo.sr.ht/~mcf/cc-issues/7
[#35]: https://todo.sr.ht/~mcf/cc-issues/35
[#36]: https://todo.sr.ht/~mcf/cc-issues/36
