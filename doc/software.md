# Software notes

This document lists some software known to build successfully, and any
special procedures necessary.

## sbase

Builds without issue as of [ef9f6f35].

[ef9f6f35]: https://git.suckless.org/sbase/commit/ef9f6f359a0762b738302ae05822514d72b70450.html

## binutils

QBE must be built with `NPred` (in `all.h`) at least 297.

On glibc systems, you must make sure to include `crtbegin.o` and
`crtend.o` from gcc at the end of `startfiles` and beginning of `endfiles`
respectively.

On musl systems, you must define `long double` to match `double` to
avoid errors in unused `static inline` functions in musl's `math.h`.

Requires several patches available here:
https://github.com/michaelforney/binutils-gdb/

- Fix function pointer subtraction in `bfd/doc/chew.c` (applied upstream).
- Skip unsupported `LDFLAGS`, only tested to work against `CXX` by
  configure, but applied to `CC` as well.
- Disable `long double` support in `_bfd_doprnt`.
- Alter some ifdefs to avoid statement expressions and VLAs.
- Implement `pex_unix_exec_child` with `posix_spawn` instead of `vfork`
  and subtle `volatile` usage.
- Make `regcomp` and `regexec` match the header declaration in usage of
  `restrict`.
- Don't declare `vasprintf` unless it was checked for and not
  found. Several subdirectories in binutils include `libiberty.h`,
  but don't use `vasprintf`, causing conflicting declarations with libc
  in usage of `restrict`.
- Make sure `config.h` is included in `arlex.c` so that the appropriate
  feature-test macros get defined to expose `strdup`.

Configure with

	./configure CC=/path/to/cc CFLAGS_FOR_BUILD=-D_GNU_SOURCE \
		--disable-intl --disable-gdb --disable-plugins --disable-readline
