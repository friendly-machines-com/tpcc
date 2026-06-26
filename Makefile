
CFLAGS = -g3

%.o: %.c
	$(CXX) $(CFLAGS) $<

all: main.o parser.o cst.o
	$(CXX) -o mp $^

parser.o: parser.cc parser.h cst.h scope.h evaluator.h
cst.o: cst.cc cst.h
scope.o: scope.h
evaluator.o: evaluator.cc evaluator.h
builtins.o: builtins.cc builtins.h cst.h
