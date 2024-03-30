.POSIX:

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man
BACKEND=qbe

objdir=.
-include config.mk

.PHONY: all
all: $(objdir)/cproc $(objdir)/cproc-qbe

DRIVER_SRC=\
	driver.c\
	util.c
DRIVER_OBJ=$(DRIVER_SRC:%.c=$(objdir)/%.o)

config.h:
	./configure

$(objdir)/cproc: $(DRIVER_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(DRIVER_OBJ)

SRC=\
	attr.c\
	decl.c\
	eval.c\
	expr.c\
	init.c\
	main.c\
	map.c\
	pp.c\
	scan.c\
	scope.c\
	stmt.c\
	targ.c\
	token.c\
	tree.c\
	type.c\
	utf.c\
	util.c\
	$(BACKEND).c
OBJ=$(SRC:%.c=$(objdir)/%.o)

$(objdir)/cproc-qbe: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(objdir)/attr.o    : attr.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ attr.c
$(objdir)/decl.o    : decl.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ decl.c
$(objdir)/driver.o  : driver.c  util.h config.h   $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ driver.c
$(objdir)/eval.o    : eval.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ eval.c
$(objdir)/expr.o    : expr.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ expr.c
$(objdir)/init.o    : init.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ init.c
$(objdir)/main.o    : main.c    util.h cc.h arg.h $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ main.c
$(objdir)/map.o     : map.c     util.h            $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ map.c
$(objdir)/pp.o      : pp.c      util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ pp.c
$(objdir)/qbe.o     : qbe.c     util.h cc.h ops.h $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ qbe.c
$(objdir)/scan.o    : scan.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ scan.c
$(objdir)/scope.o   : scope.c   util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ scope.c
$(objdir)/stmt.o    : stmt.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ stmt.c
$(objdir)/targ.o    : targ.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ targ.c
$(objdir)/token.o   : token.c   util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ token.c
$(objdir)/tree.o    : tree.c    util.h            $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ tree.c
$(objdir)/type.o    : type.c    util.h cc.h       $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ type.c
$(objdir)/utf.o     : utf.c     utf.h             $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ utf.c
$(objdir)/util.o    : util.c    util.h            $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ util.c

# Make sure stage2 and stage3 binaries are stripped by adding -s to
# LDFLAGS. Otherwise they will contain paths to object files, which
# differ between stages.

.PHONY: stage2
stage2: all
	@mkdir -p $@
	$(MAKE) objdir=$@ stagedeps='cproc cproc-qbe' CC=$(objdir)/cproc LDFLAGS='$(LDFLAGS) -s'

.PHONY: stage3
stage3: stage2
	@mkdir -p $@
	$(MAKE) objdir=$@ stagedeps='stage2/cproc stage2/cproc-qbe' CC=$(objdir)/stage2/cproc LDFLAGS='$(LDFLAGS) -s'

.PHONY: bootstrap
bootstrap: stage2 stage3
	cmp stage2/cproc stage3/cproc
	cmp stage2/cproc-qbe stage3/cproc-qbe

.PHONY: check
check: all
	@CCQBE=./cproc-qbe ./runtests

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(objdir)/cproc $(objdir)/cproc-qbe $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	cp cproc.1 $(DESTDIR)$(MANDIR)/man1

.PHONY: clean
clean:
	rm -rf cproc $(DRIVER_OBJ) cproc-qbe $(OBJ) stage2 stage3
