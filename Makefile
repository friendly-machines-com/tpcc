
CXXFLAGS = -g3 -std=c++20 -Wall

all: mp

%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

mp: src/main.o src/parser.o src/cst.o src/directive_expr.o src/frame.o src/types.o src/evaluator.o src/builtins.o src/units.o src/emit.o src/diagnostic.o
	$(CXX) -o $@ $^

src/main.o: src/main.cc src/parser.h src/types.h src/units.h src/emit.h
src/parser.o: src/parser.cc src/parser.h src/cst.h src/directive_expr.h src/frame.h src/types.h src/evaluator.h src/units.h src/emit.h
src/cst.o: src/cst.cc src/cst.h
src/directive_expr.o: src/directive_expr.cc src/directive_expr.h
src/frame.o: src/frame.cc src/frame.h src/types.h src/cst.h
src/types.o: src/types.cc src/types.h src/builtins.h
src/evaluator.o: src/evaluator.cc src/evaluator.h src/cst.h src/frame.h src/types.h
src/builtins.o: src/builtins.cc src/builtins.h src/cst.h
src/units.o: src/units.cc src/units.h src/frame.h
src/emit.o: src/emit.cc src/emit.h src/cst.h src/types.h src/builtins.h
src/diagnostic.o: src/diagnostic.cc src/diagnostic.h src/types.h src/cst.h src/frame.h src/builtins.h

clean:
	rm -f src/*.o

distclean: clean
	rm -f mp core

test-packed-record: mp
	sh tests/test_packed_record.sh

test-case-statement: mp
	sh tests/test_case_statement.sh

test-math-intrinsics: mp
	sh tests/test_math_intrinsics.sh

test-loop-control: mp
	sh tests/test_loop_control.sh

test-generic-intrinsics: mp
	sh tests/test_generic_intrinsics.sh

test-pos-builtin: mp
	sh tests/test_pos_builtin.sh

test-copy-builtin: mp
	sh tests/test_copy_builtin.sh

test-delete-builtin: mp
	sh tests/test_delete_builtin.sh

test-insert-builtin: mp
	sh tests/test_insert_builtin.sh

test-properties: mp
	sh tests/test_properties.sh

test: test-packed-record test-case-statement test-math-intrinsics test-loop-control test-generic-intrinsics test-pos-builtin test-copy-builtin test-delete-builtin test-insert-builtin test-properties

.PHONY: all clean distclean test test-packed-record test-case-statement test-math-intrinsics test-loop-control test-generic-intrinsics test-pos-builtin test-copy-builtin test-delete-builtin test-insert-builtin test-properties
