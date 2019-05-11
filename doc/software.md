# Software notes

This document lists some software known to build successfully, and any
special procedures necessary.

## sbase

Builds without issue as of [ef9f6f35].

[ef9f6f35]: https://git.suckless.org/sbase/commit/ef9f6f359a0762b738302ae05822514d72b70450.html

## mcpp

Builds without issue.

## binutils

QBE must be built with `NPred` (in `all.h`) at least 297, or patched to
use dynamic arrays for phi predecessors ([f6a7d135]).

On glibc systems, you must make sure to include `crtbegin.o` and
`crtend.o` from gcc at the end of `startfiles` and beginning of `endfiles`
respectively.

On musl systems, you must define `long double` to match `double` (as
below) to avoid errors in unused `static inline` functions in musl's
`math.h`. Note: this is a hack and won't be ABI-incompatible with musl;
things will break if any functions with `long double` get called.

```diff
-struct type typeldouble = FLTTYPE(TYPELDOUBLE, 16, NULL);  // XXX: not supported by qbe
+struct type typeldouble = FLTTYPE(TYPELDOUBLE, 8, &f64);
```

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

[f6a7d135]: https://git.sr.ht/~mcf/qbe/commit/f6a7d135d54f5281547f20cc4f72a5e85862157c

## zstd

Requires disabling CPU identification inline assembly and deprecation
warnings.

```diff
diff --git a/lib/common/cpu.h b/lib/common/cpu.h
index 5f0923fc..10dd7d7f 100644
--- a/lib/common/cpu.h
+++ b/lib/common/cpu.h
@@ -52,6 +52,7 @@ MEM_STATIC ZSTD_cpuid_t ZSTD_cpuid(void) {
             f7c = (U32)reg[2];
         }
     }
+#elif 1
 #elif defined(__i386__) && defined(__PIC__) && !defined(__clang__) && defined(__GNUC__)
     /* The following block like the normal cpuid branch below, but gcc
      * reserves ebx for use of its pic register so we must specially
```

Build with

	make CC=/path/to/cc CFLAGS='-DZDICT_DISABLE_DEPRECATE_WARNINGS -DZBUFF_DISABLE_DEPRECATE_WARNINGS' zstd

Some tests fail, which still need to be investigated.
