driver.o: driver.c util.h config.h
util.o: util.c util.h
decl.o: decl.c util.h backend.h decl.h expr.h htab.h init.h pp.h scope.h \
 stmt.h token.h type.h
eval.o: eval.c util.h backend.h decl.h eval.h expr.h token.h type.h
expr.o: expr.c util.h decl.h eval.h expr.h init.h pp.h scope.h token.h \
 type.h
htab.o: htab.c util.h htab.h
init.o: init.c util.h decl.h expr.h init.h pp.h token.h type.h
main.o: main.c util.h arg.h decl.h pp.h scope.h token.h
pp.o: pp.c util.h pp.h scan.h token.h
scan.o: scan.c util.h scan.h token.h
scope.o: scope.c util.h decl.h htab.h scope.h type.h
siphash.o: siphash.c
stmt.o: stmt.c util.h backend.h decl.h expr.h pp.h scope.h stmt.h token.h \
 type.h
tree.o: tree.c util.h tree.h
token.o: token.c util.h token.h
type.o: type.c util.h backend.h type.h
util.o: util.c util.h
qbe.o: qbe.c util.h backend.h decl.h eval.h expr.h htab.h init.h scope.h \
 token.h tree.h type.h ops.h
