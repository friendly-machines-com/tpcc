
CXXFLAGS = -g3 -std=c++17 -Wall

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $<

all: mp

mp: main.o parser.o cst.o frame.o evaluator.o builtins.o
	$(CXX) -o $@ $^

main.o: main.cc parser.h
parser.o: parser.cc parser.h cst.h frame.h evaluator.h
cst.o: cst.cc cst.h
frame.o: frame.cc frame.h cst.h
evaluator.o: evaluator.cc evaluator.h cst.h frame.h
builtins.o: builtins.cc builtins.h cst.h

clean:
	rm -f *.o mp core

.PHONY: all clean
