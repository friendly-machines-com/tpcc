
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

test-ifopt: mp
	sh tests/test_ifopt.sh

test-directive-push-pop: mp
	sh tests/test_directive_push_pop.sh

test-directive-diagnostics: mp
	sh tests/test_directive_diagnostics.sh

test-case-statement: mp
	sh tests/test_case_statement.sh

test-math-intrinsics: mp
	sh tests/test_math_intrinsics.sh

test-loop-control: mp
	sh tests/test_loop_control.sh

test-empty-statement: mp
	sh tests/test_empty_statement.sh

test-generic-intrinsics: mp
	sh tests/test_generic_intrinsics.sh

test-length-native-types: mp
	sh tests/test_length_native_types.sh

test-pos-builtin: mp
	sh tests/test_pos_builtin.sh

test-copy-builtin: mp
	sh tests/test_copy_builtin.sh

test-chr-builtin: mp
	sh tests/test_chr_builtin.sh

test-str-builtin: mp
	sh tests/test_str_builtin.sh

test-val-builtin: mp
	sh tests/test_val_builtin.sh

test-octstr-builtin: mp
	sh tests/test_octstr_builtin.sh

test-strlen-builtin: mp
	sh tests/test_strlen_builtin.sh

test-getmem-builtin: mp
	sh tests/test_getmem_builtin.sh

test-move-builtin: mp
	sh tests/test_move_builtin.sh

test-comparechar-builtin: mp
	sh tests/test_comparechar_builtin.sh

test-sizeof-layout: mp
	sh tests/test_sizeof_layout.sh

test-set-literals: mp
	sh tests/test_set_literals.sh

test-include-exclude-builtin: mp
	sh tests/test_include_exclude_builtin.sh

test-writable-cast: mp
	sh tests/test_writable_cast.sh

test-write-builtin: mp
	sh tests/test_write_builtin.sh

test-runerror-halt: mp
	sh tests/test_runerror_halt.sh

test-unit-lifecycle: mp
	sh tests/test_unit_lifecycle.sh

test-unit-uses-scope: mp
	sh tests/test_unit_uses_scope.sh

test-delete-builtin: mp
	sh tests/test_delete_builtin.sh

test-insert-builtin: mp
	sh tests/test_insert_builtin.sh

test-properties: mp
	sh tests/test_properties.sh

test-function-pointers: mp
	sh tests/test_function_pointers.sh

test-single-type: mp
	sh tests/test_single_type.sh

test-shortstring-type: mp
	sh tests/test_shortstring_type.sh

test-typed-const-aggregates: mp
	sh tests/test_typed_const_aggregates.sh

test-explicit-enum-values: mp
	sh tests/test_explicit_enum_values.sh

test-type-block-publication: mp
	sh tests/test_type_block_publication.sh

test-file-types: mp
	sh tests/test_file_types.sh

test-classtype-emission: mp
	sh tests/test_classtype_emission.sh

test-metaclass-lifecycle: mp
	sh tests/test_metaclass_lifecycle.sh

test-metaclass-construction: mp
	sh tests/test_metaclass_construction.sh

test-untyped-pointer-dereference: mp
	sh tests/test_untyped_pointer_dereference.sh

test-ansistring-pointer-cast: mp
	sh tests/test_ansistring_pointer_cast.sh

test-old-file-api: mp
	sh tests/test_old_file_api.sh

test-try-statements: mp
	sh tests/test_try_statements.sh

test-exception-handlers: mp
	sh tests/test_exception_handlers.sh

test-integer-not: mp
	sh tests/test_integer_not.sh

test-pointer-equality: mp
	sh tests/test_pointer_equality.sh

test-sysutils-exception: mp
	sh tests/test_sysutils_exception.sh

test-old-object-lifecycle: mp
	sh tests/test_old_object_lifecycle.sh

test-incomplete-type-resolution: mp
	sh tests/test_incomplete_type_resolution.sh

test-final-methods: mp
	sh tests/test_final_methods.sh

test-abstract-methods: mp
	sh tests/test_abstract_methods.sh

test-abstract-classes: mp
	sh tests/test_abstract_classes.sh

test-class-static-methods: mp
	sh tests/test_class_static_methods.sh

test-interface-guid: mp
	sh tests/test_interface_guid.sh

test-interfaces-directive: mp
	sh tests/test_interfaces_directive.sh

test-type-identity-and-compatibility: mp
	sh tests/test_type_identity_and_compatibility.sh

test-operator-argument-matching: mp
	sh tests/test_operator_argument_matching.sh

test-managed-types-and-iteration: mp
	sh tests/test_managed_types_and_iteration.sh

test-overflow-checking: mp
	sh tests/test_overflow_checking.sh

test-custom-checked-operators: mp
	sh tests/test_custom_checked_operators.sh

test-overflow-constants: mp
	sh tests/test_overflow_constants.sh

test-index-range-checking: mp
	sh tests/test_index_range_checking.sh

test-subrange-carriers: mp
	sh tests/test_subrange_carriers.sh

test-implicit-ordinal-narrowing: mp
	sh tests/test_implicit_ordinal_narrowing.sh

test-implicit-conversion-single-edge: mp
	sh tests/test_implicit_conversion_single_edge.sh

test-implicit-real-narrowing: mp
	sh tests/test_implicit_real_narrowing.sh

test: test-packed-record test-ifopt test-directive-push-pop test-directive-diagnostics test-case-statement test-math-intrinsics test-loop-control test-empty-statement test-generic-intrinsics test-length-native-types test-pos-builtin test-copy-builtin test-chr-builtin test-str-builtin test-val-builtin test-octstr-builtin test-strlen-builtin test-getmem-builtin test-move-builtin test-comparechar-builtin test-sizeof-layout test-set-literals test-include-exclude-builtin test-writable-cast test-write-builtin test-runerror-halt test-unit-lifecycle test-unit-uses-scope test-delete-builtin test-insert-builtin test-properties test-function-pointers test-single-type test-shortstring-type test-typed-const-aggregates test-explicit-enum-values test-type-block-publication test-file-types test-classtype-emission test-metaclass-lifecycle test-metaclass-construction test-untyped-pointer-dereference test-ansistring-pointer-cast test-old-file-api test-try-statements test-exception-handlers test-integer-not test-pointer-equality test-sysutils-exception test-old-object-lifecycle test-incomplete-type-resolution test-final-methods test-abstract-methods test-abstract-classes test-class-static-methods test-interface-guid test-interfaces-directive test-type-identity-and-compatibility test-operator-argument-matching test-managed-types-and-iteration test-overflow-checking test-custom-checked-operators test-overflow-constants test-index-range-checking test-subrange-carriers test-implicit-ordinal-narrowing test-implicit-conversion-single-edge test-implicit-real-narrowing

.PHONY: all clean distclean test test-packed-record test-ifopt test-directive-push-pop test-directive-diagnostics test-case-statement test-math-intrinsics test-loop-control test-empty-statement test-generic-intrinsics test-length-native-types test-pos-builtin test-copy-builtin test-chr-builtin test-str-builtin test-val-builtin test-octstr-builtin test-strlen-builtin test-getmem-builtin test-move-builtin test-comparechar-builtin test-sizeof-layout test-set-literals test-include-exclude-builtin test-writable-cast test-write-builtin test-runerror-halt test-unit-lifecycle test-unit-uses-scope test-delete-builtin test-insert-builtin test-properties test-function-pointers test-single-type test-shortstring-type test-typed-const-aggregates test-explicit-enum-values test-type-block-publication test-file-types test-classtype-emission test-metaclass-lifecycle test-metaclass-construction test-untyped-pointer-dereference test-ansistring-pointer-cast test-old-file-api test-try-statements test-exception-handlers test-integer-not test-pointer-equality test-sysutils-exception test-old-object-lifecycle test-incomplete-type-resolution test-final-methods test-abstract-methods test-abstract-classes test-class-static-methods test-interface-guid test-interfaces-directive test-type-identity-and-compatibility test-operator-argument-matching test-managed-types-and-iteration test-overflow-checking test-custom-checked-operators test-overflow-constants test-index-range-checking test-subrange-carriers test-implicit-ordinal-narrowing test-implicit-conversion-single-edge test-implicit-real-narrowing
