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
	init.c\
	main.c\
	map.c\
	pp.c\
	scan.c\
	scope.c\
	siphash.c\
	stmt.c\
	token.c\
	tree.c\
	type.c\
	util.c\
	$(BACKEND).c
OBJ=$(SRC:%.c=$(objdir)/%.o)

$(objdir)/cc-qbe: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(objdir)/decl.o    : decl.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ decl.c
$(objdir)/driver.o  : driver.c  $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ driver.c
$(objdir)/eval.o    : eval.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ eval.c
$(objdir)/expr.o    : expr.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ expr.c
$(objdir)/init.o    : init.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ init.c
$(objdir)/main.o    : main.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ main.c
$(objdir)/map.o     : map.c     $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ map.c
$(objdir)/pp.o      : pp.c      $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ pp.c
$(objdir)/qbe.o     : qbe.c     $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ qbe.c
$(objdir)/scan.o    : scan.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ scan.c
$(objdir)/scope.o   : scope.c   $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ scope.c
$(objdir)/siphash.o : siphash.c $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ siphash.c
$(objdir)/stmt.o    : stmt.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ stmt.c
$(objdir)/token.o   : token.c   $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ token.c
$(objdir)/tree.o    : tree.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ tree.c
$(objdir)/type.o    : type.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ type.c
$(objdir)/util.o    : util.c    $(stagedeps) ; $(CC) $(CFLAGS) -c -o $@ util.c

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
check: all
	@CCQBE=./cc-qbe ./runtests

.PHONY: clean
clean:
	rm -rf cc $(DRIVER_OBJ) cc-qbe $(OBJ) stage2 stage3

deps.mk: $(DRIVER_SRC) $(SRC) config.h
	for src in $(DRIVER_SRC) $(SRC); do $(CC) $(CFLAGS) -MM -MT "\$$(objdir)/$${src%.c}.o" "$$src"; done >$@
-include deps.mk
