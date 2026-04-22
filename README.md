(mirrored on [GitHub][GitHub mirror] and [Codeberg][Codeberg mirror])

[![builds.sr.ht status](https://builds.sr.ht/~mcf/cproc/commits/master.svg)](https://builds.sr.ht/~mcf/cproc/commits/master)

`cproc` is a C compiler using [QBE] as a backend, released under
the [ISC] license. It implements most of C11 as well as many [C23]
features. Additionally, it implements a few [GNU C extensions].

It is currently able to build a lot of C software, though occasionally
requires small patches to fix C conformance issues.

It was inspired by several other small C compilers including [8cc],
[c], [lacc], and [scc].

## Goals

`cproc` aims to closely follow the C standard, but also to be a
practical tool able compile many real-world software packages.
Sometimes, these goals can be at ends with each other, since there
is a *lot* of non-conforming C code out there.

The main philosophy regarding C extensions is this:

- If the code could be made portable with some small tweaks that
  do not affect performance or readability, then the code should
  be patched accordingly.
- Only when an extension is widely used *and* cannot be easily
  transformed into a portable equivalent is it considered for
  implementation. Ideally, it should also be accompanied with a
  proposal to WG14 so that its semantics are well specified.

By following these guidelines, hopefully `cproc` can help shrink
the gap between what's required to implement the C specification
and what's required to compile real C software.

`cproc` does not implement multiple versions of C. Instead, code
affected by breaking changes in new C versions (such as the new
keywords introduced in C23) must be updated or patched to be
compatible with the new version.

## Requirements

The compiler itself is written in standard C99 and can be built with
any conforming C99 compiler.

The POSIX driver depends on POSIX.1-2008 interfaces, and the `Makefile`
requires a POSIX-compatible make(1).

At runtime, you will need QBE, an assembler, and a linker for the
target system. Since the preprocessor is not fully implemented, an
external one is currently required as well.

## Supported targets

All architectures supported by QBE should work (currently x86\_64,
aarch64, and riscv64).

The following targets are tested by the continuous build and known to
bootstrap and pass all tests:

- `x86_64-linux-musl`
- `x86_64-linux-gnu`
- `x86_64-freebsd`
- `aarch64-linux-musl`
- `aarch64-linux-gnu`
- `riscv64-linux-gnu`

## Building

Run `./configure` to create a `config.h` and `config.mk` appropriate for
your system. If your system is not supported by the configure script,
you can create these files manually. `config.h` should define several
string arrays (`static char *[]`):

- **`startfiles`**: Objects to pass to the linker at the beginning of
  the link command.
- **`endfiles`**: Objects to pass to the linker at the end of the link
  command (including libc).
- **`preprocesscmd`**: The preprocessor command, and any necessary flags
  for the target system.
- **`codegencmd`**: The QBE command, and possibly explicit target flags.
- **`assemblecmd`**: The assembler command.
- **`linkcmd`**: The linker command.

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

- Digraph sequences ([6.4.6p3], will not be implemented).
- `volatile`-qualified types ([#7], requires qbe support).
- `long double` type ([#3], requires qbe support).
- The preprocessor is not fully implemented ([#6]).
- Generation of position independent code (i.e. shared libraries,
  modules, PIEs).

### C11

- Complex types (optional).
- Atomic types (optional).

### C23

See [C23] for a detailed breakdown of the language-level changes
from C11 to C23 as well as their current status. Notably, the
following are not yet implemented:

- `constexpr`
- `auto`
- `#embed`

### GNU C extensions

- Inline assembly ([#5], requires qbe support).
- Statement expressions ([#20], unlikely to be implemented without
  specification and WG14 acceptance).

## Mailing list

There is a mailing list at [~mcf/cproc@lists.sr.ht]. Feel free to
use it for general discussion, questions, patches, or bug reports
(if you don't have an sr.ht account).

If you don't hear a response, please don't hesitate to bump your
thread.

## Issue tracker

Please report any issues to [~mcf/cproc@todo.sr.ht].

## Contributing

Patches are greatly appreciated. Send them to the mailing list
(preferred), or as pull-requests on the [Codeberg mirror].

[QBE]: https://c9x.me/compile/
[ISC]: https://git.sr.ht/~mcf/cproc/blob/master/LICENSE
[C23]: https://man.sr.ht/~mcf/cproc/doc/c23.md
[GNU C extensions]: https://man.sr.ht/~mcf/cproc/doc/extensions.md
[building software]: https://man.sr.ht/~mcf/cproc/doc/software.md
[8cc]: https://github.com/rui314/8cc
[c]: https://github.com/andrewchambers/c
[lacc]: https://github.com/larmel/lacc
[scc]: http://www.simple-cc.org/
[6.4.6p3]: http://port70.net/~nsz/c/c11/n1570.html#6.4.6p3
[#3]: https://todo.sr.ht/~mcf/cproc/3
[#5]: https://todo.sr.ht/~mcf/cproc/5
[#6]: https://todo.sr.ht/~mcf/cproc/6
[#7]: https://todo.sr.ht/~mcf/cproc/7
[#20]: https://todo.sr.ht/~mcf/cproc/20
[~mcf/cproc@lists.sr.ht]: https://lists.sr.ht/~mcf/cproc
[~mcf/cproc@todo.sr.ht]: https://todo.sr.ht/~mcf/cproc
[GitHub mirror]: https://github.com/michaelforney/cproc
[Codeberg mirror]: https://codeberg.org/mcf/cproc
