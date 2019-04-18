[![builds.sr.ht status](https://builds.sr.ht/~mcf/cc.svg)](https://builds.sr.ht/~mcf/cc)

This is a [C11] compiler using [QBE] as a backend. It is released under
the [ISC] license.

Several GNU C [extensions] are also implemented.

There is still much to do and some parts of the code are a little rough,
but it currently implements most of the language, is self-hosting,
and capable of building some useful software.

It was inspired by several other small C compilers including [8cc],
[c], and [scc].

## Requirements

The compiler itself is written in standard C11 and can be built with
any conforming C11 compiler.

The POSIX driver depends on POSIX.1-2008 interfaces, and the `Makefile`
requires a POSIX-compatible make(1).

At runtime, you will need QBE, an assembler, and a linker for the
target system. Since the preprocessor is not yet implemented, an external
one is currently required as well.

## Building

You will need to create a `config.h` appropriate for the target system. If
missing, a default version will be created from `config.def.h`,
which should work for most glibc systems, or musl systems with
`-D 'DYNAMIC_LINKER="/lib/ld-musl-x86_64.so.1"'`.

You may also want to customize your environment or `config.mk` with the
appropriate `CC`, `CFLAGS` and `LDFLAGS`.

Once this is done, you can build with

	make

### Bootstrap

The `Makefile` includes several other targets that can be used for
bootstrapping. These targets require the ability to run the tools
specified in `config.h`.

- **`stage2`**: Build the compiler with the initial (`stage1`) output.
- **`stage3`**: Build the compiler with the `stage2` output.
- **`bootstrap`**: Build the `stage2` and `stage3` compilers, and verify
  that they are byte-wise identical.

## What's missing

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
- `_Generic` selection ([#44]).
- Currently only `x86_64` is supported and tested, though QBE also
  supports `aarch64`, so it is possible that it works as well.

## Issue tracker

Please report any issues to https://todo.sr.ht/~mcf/cc-issues.

[QBE]: https://c9x.me/compile/
[C11]: http://port70.net/~nsz/c/c11/n1570.html
[ISC]: https://git.sr.ht/~mcf/cc/blob/master/LICENSE
[extensions]: https://git.sr.ht/~mcf/cc/tree/master/doc/extensions.md
[8cc]: https://github.com/rui314/8cc
[c]: https://github.com/andrewchambers/c
[scc]: http://www.simple-cc.org/
[5.2.1.1]: http://port70.net/~nsz/c/c11/n1570.html#5.2.1.1
[6.4.6p3]: http://port70.net/~nsz/c/c11/n1570.html#6.4.6p3
[#1]: https://todo.sr.ht/~mcf/cc-issues/1
[#3]: https://todo.sr.ht/~mcf/cc-issues/3
[#5]: https://todo.sr.ht/~mcf/cc-issues/5
[#6]: https://todo.sr.ht/~mcf/cc-issues/6
[#7]: https://todo.sr.ht/~mcf/cc-issues/7
[#35]: https://todo.sr.ht/~mcf/cc-issues/35
[#36]: https://todo.sr.ht/~mcf/cc-issues/36
[#44]: https://todo.sr.ht/~mcf/cc-issues/44
