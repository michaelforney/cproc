.POSIX:

BACKEND=qbe

srcdir=.
-include $(srcdir)/config.mk

.PHONY: all
all: cc cc-qbe

DRIVER_SRC=\
	driver.c\
	util.c
DRIVER_OBJ=$(DRIVER_SRC:.c=.o)

config.h:
	@cp config.def.h $@

cc: $(DRIVER_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(DRIVER_OBJ)

SRC=\
	decl.c\
	eval.c\
	expr.c\
	htab.c\
	init.c\
	main.c\
	pp.c\
	scan.c\
	scope.c\
	siphash.c\
	stmt.c\
	tree.c\
	token.c\
	type.c\
	util.c\
	$(BACKEND).c
OBJ=$(SRC:.c=.o)

cc-qbe: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

decl.o    : $(srcdir)/decl.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
driver.o  : $(srcdir)/driver.c  $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
eval.o    : $(srcdir)/eval.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
expr.o    : $(srcdir)/expr.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
htab.o    : $(srcdir)/htab.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
init.o    : $(srcdir)/init.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
main.o    : $(srcdir)/main.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
pp.o      : $(srcdir)/pp.c      $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
scan.o    : $(srcdir)/scan.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
scope.o   : $(srcdir)/scope.c   $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
siphash.o : $(srcdir)/siphash.c $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
stmt.o    : $(srcdir)/stmt.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
tree.o    : $(srcdir)/tree.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
token.o   : $(srcdir)/token.c   $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
type.o    : $(srcdir)/type.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
util.o    : $(srcdir)/util.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
qbe.o     : $(srcdir)/qbe.c     $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<

.PHONY: stage2
stage2: cc cc-qbe
	@mkdir -p $@
	$(MAKE) -C $@ -f ../$(srcdir)/Makefile srcdir=../$(srcdir) stagedeps='../cc ../cc-qbe' CC=../cc
.PHONY: stage3
stage3: stage2
	@mkdir -p $@
	$(MAKE) -C $@ -f ../$(srcdir)/Makefile srcdir=../$(srcdir) stagedeps='../stage2/cc ../stage2/cc-qbe' CC=../stage2/cc

.PHONY: bootstrap
bootstrap: stage2 stage3
	cmp stage2/cc stage3/cc
	cmp stage2/cc-qbe stage3/cc-qbe

.PHONY: check
check: cc cc-qbe
	@./runtests

.PHONY: clean
clean:
	rm -rf cc $(DRIVER_OBJ) cc-qbe $(OBJ) stage2 stage3

deps.mk: $(DRIVER_SRC) $(SRC) config.h
	$(CC) $(CFLAGS) -MM $(DRIVER_SRC) $(SRC) >$@
-include deps.mk
