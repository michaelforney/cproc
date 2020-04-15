# Software notes

This document lists some software known to build successfully, and any
special procedures necessary.

## oasis

One of the main goals of cproc is to compile the entire oasis linux
system (excluding kernel and libc). This is a work in progress, but many
packages have patches to fix various ISO C conformance issues, enabling
them to be built.

See the [oasis #13] for the current status of this effort, and the
patch directories in the [oasis package tree] for any patches used.

[oasis #13]: https://github.com/oasislinux/oasis/issues/13
[oasis package tree]: https://github.com/oasislinux/oasis/tree/master/pkg

## binutils

On musl systems, you must define `long double` to match `double` (as
below) to avoid errors in unused `static inline` functions in musl's
`math.h`. Note: this is a hack and won't be ABI-incompatible with musl;
things will break if any functions with `long double` get called.

```diff
-struct type typeldouble = FLTTYPE(TYPELDOUBLE, 16, NULL);  // XXX: not supported by qbe
+struct type typeldouble = FLTTYPE(TYPELDOUBLE, 8, &f64);
```

Requires several patches available here:
https://github.com/michaelforney/binutils-gdb/tree/cproc-fixes

- Disable `long double` support in `_bfd_doprnt`.
- Implement `pex_unix_exec_child` with `posix_spawn` instead of `vfork`
  and subtle `volatile` usage.
- Provide fallback definitions for `_libctf_*_` attribute macros,
  accidentally omitted when merged into binutils ([#25120]).
- Remove usage of `__thread`, only necessary when linking libctf
  externally ([#25120]).

Configure with

```
./configure --disable-gdb --disable-plugins --disable-readline
```

[#25120]: https://sourceware.org/bugzilla/show_bug.cgi?id=25120

## gcc 4.7

Requires a number of patches available here:
https://github.com/michaelforney/gcc/tree/cproc-fixes

Also requires gmp headers modified for C99 inline semantics:
https://hg.sr.ht/~mcf/gmp-6.1/rev/53195faa26dfeafeacd57f54035373988e2a16a3

Build with

```
git clone -b cc-fixes https://github.com/michaelforney/gcc
cd gcc
hg clone https://hg.sr.ht/~mcf/gmp-6.1 gmp
(cd gmp && aux_dir=. ltdl_dir=. ./.bootstrap)
./configure --disable-multilib --disable-bootstrap --disable-lto --enable-languages=c,c++
make
```

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

	make CC=cproc CFLAGS='-DZDICT_DISABLE_DEPRECATE_WARNINGS -DZBUFF_DISABLE_DEPRECATE_WARNINGS' zstd

## st

Requires a few changes to avoid wide string literals and including
`math.h` on musl targets.

```diff
diff --git a/config.def.h b/config.def.h
index 482901e..bd963ae 100644
--- a/config.def.h
+++ b/config.def.h
@@ -32,7 +32,7 @@ static float chscale = 1.0;
  *
  * More advanced example: L" `'\"()[]{}"
  */
-wchar_t *worddelimiters = L" ";
+wchar_t *worddelimiters = (wchar_t[]){' ', 0};
 
 /* selection timeouts (in milliseconds) */
 static unsigned int doubleclicktimeout = 300;
diff --git a/x.c b/x.c
index 5828a3b..6e006cc 100644
--- a/x.c
+++ b/x.c
@@ -1,6 +1,5 @@
 /* See LICENSE for license details. */
 #include <errno.h>
-#include <math.h>
 #include <limits.h>
 #include <locale.h>
 #include <signal.h>
@@ -15,6 +14,8 @@
 #include <X11/Xft/Xft.h>
 #include <X11/XKBlib.h>
 
+float ceilf(float);
+
 static char *argv0;
 #include "arg.h"
 #include "st.h"
```
