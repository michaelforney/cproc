.POSIX:

BACKEND=qbe

objdir=.
-include config.mk

.PHONY: all
all: $(objdir)/cc $(objdir)/cc-qbe

DRIVER_SRC=\
	driver.c\
	util.c
DRIVER_OBJ=$(DRIVER_SRC:%.c=$(objdir)/%.o)

config.h:
	@cp config.def.h $@

$(objdir)/cc: $(DRIVER_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(DRIVER_OBJ)

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
OBJ=$(SRC:%.c=$(objdir)/%.o)

$(objdir)/cc-qbe: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(objdir)/decl.o    : decl.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/driver.o  : driver.c  $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/eval.o    : eval.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/expr.o    : expr.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/htab.o    : htab.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/init.o    : init.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/main.o    : main.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/pp.o      : pp.c      $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/scan.o    : scan.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/scope.o   : scope.c   $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/siphash.o : siphash.c $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/stmt.o    : stmt.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/tree.o    : tree.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/token.o   : token.c   $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/type.o    : type.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/util.o    : util.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<
$(objdir)/qbe.o     : qbe.c     $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ $<

.PHONY: stage2
stage2: all
	@mkdir -p $@
	$(MAKE) objdir=$@ stagedeps='cc cc-qbe' CC=$(objdir)/cc
	strip $@/cc $@/cc-qbe

.PHONY: stage3
stage3: stage2
	@mkdir -p $@
	$(MAKE) objdir=$@ stagedeps='stage2/cc stage2/cc-qbe' CC=$(objdir)/stage2/cc
	strip $@/cc $@/cc-qbe

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
