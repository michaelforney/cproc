driver.o: driver.c util.h config.h
util.o: util.c util.h
decl.o: decl.c util.h cc.h htab.h
eval.o: eval.c util.h cc.h
expr.o: expr.c util.h cc.h
htab.o: htab.c util.h htab.h
init.o: init.c util.h cc.h
main.o: main.c util.h arg.h cc.h
pp.o: pp.c util.h cc.h
scan.o: scan.c util.h cc.h
scope.o: scope.c util.h cc.h htab.h
siphash.o: siphash.c
stmt.o: stmt.c util.h cc.h
tree.o: tree.c util.h tree.h
token.o: token.c util.h cc.h
type.o: type.c util.h cc.h
util.o: util.c util.h
qbe.o: qbe.c util.h cc.h htab.h tree.h ops.h
