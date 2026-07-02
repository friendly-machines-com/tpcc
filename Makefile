
CXXFLAGS = -g3 -std=c++20 -Wall

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $<

all: mp

mp: main.o parser.o cst.o frame.o types.o evaluator.o builtins.o units.o emit.o
	$(CXX) -o $@ $^

main.o: main.cc parser.h units.h emit.h
parser.o: parser.cc parser.h cst.h frame.h types.h evaluator.h units.h emit.h
cst.o: cst.cc cst.h
frame.o: frame.cc frame.h types.h cst.h
types.o: types.cc types.h
evaluator.o: evaluator.cc evaluator.h cst.h frame.h types.h
builtins.o: builtins.cc builtins.h cst.h
units.o: units.cc units.h frame.h
emit.o: emit.cc emit.h cst.h types.h builtins.h

clean:
	rm -f *.o mp core

.PHONY: all clean
