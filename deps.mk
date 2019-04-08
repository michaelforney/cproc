$(objdir)/driver.o: driver.c util.h config.h
$(objdir)/util.o: util.c util.h
$(objdir)/decl.o: decl.c util.h cc.h htab.h
$(objdir)/eval.o: eval.c util.h cc.h
$(objdir)/expr.o: expr.c util.h cc.h
$(objdir)/htab.o: htab.c util.h htab.h
$(objdir)/init.o: init.c util.h cc.h
$(objdir)/main.o: main.c util.h arg.h cc.h
$(objdir)/pp.o: pp.c util.h cc.h
$(objdir)/scan.o: scan.c util.h cc.h
$(objdir)/scope.o: scope.c util.h cc.h htab.h
$(objdir)/siphash.o: siphash.c
$(objdir)/stmt.o: stmt.c util.h cc.h
$(objdir)/tree.o: tree.c util.h tree.h
$(objdir)/token.o: token.c util.h cc.h
$(objdir)/type.o: type.c util.h cc.h
$(objdir)/util.o: util.c util.h
$(objdir)/qbe.o: qbe.c util.h cc.h htab.h tree.h ops.h
