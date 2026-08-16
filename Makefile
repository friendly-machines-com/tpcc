
CXXFLAGS = -g3 -std=c++20 -Wall

all: mp

%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

mp: src/main.o src/parser.o src/cst.o src/directive_expr.o src/frame.o src/types.o src/evaluator.o src/numeric_constants.o src/builtins.o src/operators.o src/units.o src/emit.o src/diagnostic.o
	$(CXX) -o $@ $^

src/main.o: src/main.cc src/emit.h src/parser.h src/units.h
src/parser.o: src/parser.cc src/parser.h src/builtins.h src/cst.h src/diagnostic.h src/directive_expr.h src/emit.h src/evaluator.h src/frame.h src/operators.h src/units.h
src/cst.o: src/cst.cc src/cst.h src/builtins.h src/diagnostic.h src/evaluator.h src/frame.h src/types.h src/units.h
src/directive_expr.o: src/directive_expr.cc src/directive_expr.h
src/frame.o: src/frame.cc src/frame.h src/builtins.h src/cst.h
src/types.o: src/types.cc src/types.h src/builtins.h src/cst.h src/diagnostic.h src/evaluator.h src/frame.h src/numeric_constants.h
src/evaluator.o: src/evaluator.cc src/evaluator.h src/builtins.h src/cst.h src/numeric_constants.h src/types.h
src/numeric_constants.o: src/numeric_constants.cc src/numeric_constants.h src/types.h src/builtins.h
src/builtins.o: src/builtins.cc src/builtins.h src/cst.h src/diagnostic.h src/evaluator.h src/frame.h src/operators.h src/types.h
src/operators.o: src/operators.cc src/operators.h
src/units.o: src/units.cc src/units.h src/cst.h src/frame.h
src/emit.o: src/emit.cc src/emit.h src/builtins.h src/cst.h src/diagnostic.h src/frame.h src/operators.h src/types.h src/units.h
src/diagnostic.o: src/diagnostic.cc src/diagnostic.h src/builtins.h src/cst.h src/frame.h src/types.h src/units.h

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

test-h-directive-strings: mp
	sh tests/test_h_directive_strings.sh

test-character-code-literals: mp
	sh tests/test_character_code_literals.sh

test-directive-diagnostics: mp
	sh tests/test_directive_diagnostics.sh

test-diagnostic-context: mp
	sh tests/test_diagnostic_context.sh

test-frame-intrinsics: mp
	sh tests/test_frame_intrinsics.sh

test-binding-lookup: mp
	sh tests/test_binding_lookup.sh

test-global-overload-scope: mp
	sh tests/test_global_overload_scope.sh

test-case-statement: mp
	sh tests/test_case_statement.sh

test-math-intrinsics: mp
	sh tests/test_math_intrinsics.sh

test-loop-control: mp
	sh tests/test_loop_control.sh

test-empty-statement: mp
	sh tests/test_empty_statement.sh

test-block-comments: mp
	sh tests/test_block_comments.sh

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

test-strtoint: mp
	sh tests/test_strtoint.sh

test-inttostr: mp
	sh tests/test_inttostr.sh

test-strpas: mp
	sh tests/test_strpas.sh

test-octstr-builtin: mp
	sh tests/test_octstr_builtin.sh

test-hexstr-builtin: mp
	sh tests/test_hexstr_builtin.sh

test-paramstr-builtin: mp
	sh tests/test_paramstr_builtin.sh

test-strlen-builtin: mp
	sh tests/test_strlen_builtin.sh

test-getmem-builtin: mp
	sh tests/test_getmem_builtin.sh

test-move-builtin: mp
	sh tests/test_move_builtin.sh

test-initialize-finalize: mp
	sh tests/test_initialize_finalize.sh

test-filldword-builtin: mp
	sh tests/test_filldword_builtin.sh

test-prefetch-builtin: mp
	sh tests/test_prefetch_builtin.sh

test-comparechar-builtin: mp
	sh tests/test_comparechar_builtin.sh

test-sizeof-layout: mp
	sh tests/test_sizeof_layout.sh

test-type-projections: mp
	sh tests/test_type_projections.sh

test-set-literals: mp
	sh tests/test_set_literals.sh

test-set-arithmetic: mp
	sh tests/test_set_arithmetic.sh

test-include-exclude-builtin: mp
	sh tests/test_include_exclude_builtin.sh

test-writable-cast: mp
	sh tests/test_writable_cast.sh

test-write-builtin: mp
	sh tests/test_write_builtin.sh

test-builtin-overload-isolation: mp
	sh tests/test_builtin_overload_isolation.sh

test-ordinary-external: mp
	sh tests/test_ordinary_external.sh

test-i-directive-scoping: mp
	sh tests/test_i_directive_scoping.sh

test-io-checking: mp
	sh tests/test_io_checking.sh

test-runerror-halt: mp
	sh tests/test_runerror_halt.sh

test-unit-lifecycle: mp
	sh tests/test_unit_lifecycle.sh

test-unit-uses-scope: mp
	sh tests/test_unit_uses_scope.sh

test-symbol-resolution: mp
	sh tests/test_symbol_resolution.sh

test-delete-builtin: mp
	sh tests/test_delete_builtin.sh

test-insert-builtin: mp
	sh tests/test_insert_builtin.sh

test-properties: mp
	sh tests/test_properties.sh

test-with-designator: mp
	sh tests/test_with_designator.sh

test-function-pointers: mp
	sh tests/test_function_pointers.sh

test-function-result-designator: mp
	sh tests/test_function_result_designator.sh

test-single-type: mp
	sh tests/test_single_type.sh

test-shortstring-type: mp
	sh tests/test_shortstring_type.sh

test-swapendian-builtin: mp
	sh tests/test_swapendian_builtin.sh

test-setstring-builtin: mp
	sh tests/test_setstring_builtin.sh

test-typed-const-aggregates: mp
	sh tests/test_typed_const_aggregates.sh

test-typed-static-address-initializers: mp
	sh tests/test_typed_static_address_initializers.sh

test-explicit-enum-values: mp
	sh tests/test_explicit_enum_values.sh

test-symbolic-operator-rejection: mp
	sh tests/test_symbolic_operator_rejection.sh

test-enum-ordinal-constants: mp
	sh tests/test_enum_ordinal_constants.sh

test-type-block-publication: mp
	sh tests/test_type_block_publication.sh

test-inherited-calls: mp
	sh tests/test_inherited_calls.sh

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

test-omitted-out-byte-pointer: mp
	sh tests/test_omitted_out_byte_pointer.sh

test-ansistring-pointer-cast: mp
	sh tests/test_ansistring_pointer_cast.sh

test-old-file-api: mp
	sh tests/test_old_file_api.sh

test-named-text-input: mp
	sh tests/test_named_text_input.sh

test-file-lifecycle: mp
	sh tests/test_file_lifecycle.sh

test-file-error-state: mp
	sh tests/test_file_error_state.sh

test-file-copy-rejected: mp
	sh tests/test_file_copy_rejected.sh

test-getdir: mp
	sh tests/test_getdir.sh

test-rmdir: mp
	sh tests/test_rmdir.sh

test-environment-variable: mp
	sh tests/test_environment_variable.sh

test-get-local-time: mp
	sh tests/test_get_local_time.sh

test-file-date-to-date-time: mp
	sh tests/test_file_date_to_date_time.sh

test-decode-date-time: mp
	sh tests/test_decode_date_time.sh

test-tdatetime: mp
	sh tests/test_tdatetime.sh

test-change-file-ext: mp
	sh tests/test_change_file_ext.sh

test-delete-file: mp
	sh tests/test_delete_file.sh

test-execute-process: mp
	sh tests/test_execute_process.sh

test-unix-fpsystem: mp
	sh tests/test_unix_fpsystem.sh

test-try-statements: mp
	sh tests/test_try_statements.sh

test-exception-handlers: mp
	sh tests/test_exception_handlers.sh

test-integer-not: mp
	sh tests/test_integer_not.sh

test-shift-operators: mp
	sh tests/test_shift_operators.sh

test-pointer-equality: mp
	sh tests/test_pointer_equality.sh

test-sysutils-exception: mp
	sh tests/test_sysutils_exception.sh

test-findfirst-findnext: mp
	sh tests/test_findfirst_findnext.sh

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

test-typed-integer-constant-conversion: mp
	sh tests/test_typed_integer_constant_conversion.sh

test-operator-ranking-policies: mp
	sh tests/test_operator_ranking_policies.sh

test-overload-ranking: mp
	sh tests/test_overload_ranking.sh

test-operator-catalog: mp
	sh tests/test_operator_catalog.sh

test-managed-types-and-iteration: mp
	sh tests/test_managed_types_and_iteration.sh

test-overflow-checking: mp
	sh tests/test_overflow_checking.sh

test-q-intrinsics: mp
	sh tests/test_q_intrinsics.sh

test-q-directive-scoping: mp
	sh tests/test_q_directive_scoping.sh

test-inc-dec-operators: mp
	sh tests/test_inc_dec_operators.sh

test-pointer-arithmetic: mp
	sh tests/test_pointer_arithmetic.sh

test-custom-checked-operators: mp
	sh tests/test_custom_checked_operators.sh

test-custom-range-conversions: mp
	sh tests/test_custom_range_conversions.sh

test-custom-explicit-conversions: mp
	sh tests/test_custom_explicit_conversions.sh

test-custom-in-operator: mp
	sh tests/test_custom_in_operator.sh

test-emission-semantic-boundaries: mp
	sh tests/test_emission_semantic_boundaries.sh

test-predefined-explicit-conversions: mp
	sh tests/test_predefined_explicit_conversions.sh

test-class-pointer-conversion: mp
	sh tests/test_class_pointer_conversion.sh

test-overflow-constants: mp
	sh tests/test_overflow_constants.sh

test-index-range-checking: mp
	sh tests/test_index_range_checking.sh

test-subrange-carriers: mp
	sh tests/test_subrange_carriers.sh

test-implicit-ordinal-narrowing: mp
	sh tests/test_implicit_ordinal_narrowing.sh

test-based-integer-literals: mp
	sh tests/test_based_integer_literals.sh

test-implicit-conversion-single-edge: mp
	sh tests/test_implicit_conversion_single_edge.sh

test-implicit-real-narrowing: mp
	sh tests/test_implicit_real_narrowing.sh

test-real-constant-semantics: mp
	sh tests/test_real_constant_semantics.sh
test-absolute-alias: mp
	sh tests/test_absolute_alias.sh

test: test-predefined-explicit-conversions test-class-pointer-conversion test-custom-in-operator test-emission-semantic-boundaries test-pointer-arithmetic test-set-arithmetic test-diagnostic-context test-frame-intrinsics test-binding-lookup test-global-overload-scope test-absolute-alias
test: test-function-result-designator
test: test-file-error-state
test: test-omitted-out-byte-pointer
test: test-strtoint
test: test-inttostr
test: test-strpas
test: test-delete-file
test: test-swapendian-builtin
test: test-symbolic-operator-rejection
test: test-packed-record test-ifopt test-directive-push-pop test-h-directive-strings test-character-code-literals test-directive-diagnostics test-case-statement test-math-intrinsics test-loop-control test-empty-statement test-block-comments test-generic-intrinsics test-length-native-types test-pos-builtin test-copy-builtin test-chr-builtin test-str-builtin test-val-builtin test-octstr-builtin test-hexstr-builtin test-paramstr-builtin test-strlen-builtin test-getmem-builtin test-move-builtin test-initialize-finalize test-filldword-builtin test-prefetch-builtin test-comparechar-builtin test-sizeof-layout test-type-projections test-set-literals test-include-exclude-builtin test-writable-cast test-write-builtin test-builtin-overload-isolation test-ordinary-external test-i-directive-scoping test-io-checking test-runerror-halt test-unit-lifecycle test-unit-uses-scope test-symbol-resolution test-delete-builtin test-insert-builtin test-properties test-with-designator test-function-pointers test-single-type test-shortstring-type test-setstring-builtin test-typed-const-aggregates test-typed-static-address-initializers test-explicit-enum-values test-enum-ordinal-constants test-type-block-publication test-inherited-calls test-file-types test-classtype-emission test-metaclass-lifecycle test-metaclass-construction test-untyped-pointer-dereference test-ansistring-pointer-cast test-old-file-api test-named-text-input test-file-lifecycle test-file-copy-rejected test-getdir test-rmdir test-environment-variable test-get-local-time test-file-date-to-date-time test-decode-date-time test-tdatetime test-change-file-ext test-execute-process test-unix-fpsystem test-try-statements test-exception-handlers test-integer-not test-shift-operators test-pointer-equality test-sysutils-exception test-findfirst-findnext test-old-object-lifecycle test-incomplete-type-resolution test-final-methods test-abstract-methods test-abstract-classes test-class-static-methods test-interface-guid test-interfaces-directive test-type-identity-and-compatibility test-operator-argument-matching test-operator-ranking-policies test-overload-ranking test-operator-catalog test-managed-types-and-iteration test-overflow-checking test-q-intrinsics test-q-directive-scoping test-inc-dec-operators test-custom-checked-operators test-custom-range-conversions test-custom-explicit-conversions test-overflow-constants test-index-range-checking test-subrange-carriers test-implicit-ordinal-narrowing test-based-integer-literals test-implicit-conversion-single-edge test-implicit-real-narrowing test-real-constant-semantics

.PHONY: all clean distclean test test-packed-record test-ifopt test-directive-push-pop test-h-directive-strings test-character-code-literals test-directive-diagnostics test-binding-lookup test-global-overload-scope test-case-statement test-math-intrinsics test-loop-control test-empty-statement test-block-comments test-generic-intrinsics test-length-native-types test-pos-builtin test-copy-builtin test-chr-builtin test-str-builtin test-val-builtin test-octstr-builtin test-hexstr-builtin test-paramstr-builtin test-strlen-builtin test-getmem-builtin test-move-builtin test-initialize-finalize test-filldword-builtin test-prefetch-builtin test-comparechar-builtin test-sizeof-layout test-type-projections test-set-literals test-include-exclude-builtin test-writable-cast test-write-builtin test-builtin-overload-isolation test-ordinary-external test-i-directive-scoping test-io-checking test-runerror-halt test-unit-lifecycle test-unit-uses-scope test-symbol-resolution test-delete-builtin test-insert-builtin test-properties test-with-designator test-function-pointers test-single-type test-shortstring-type test-setstring-builtin test-typed-const-aggregates test-typed-static-address-initializers test-explicit-enum-values test-enum-ordinal-constants test-type-block-publication test-inherited-calls test-file-types test-classtype-emission test-metaclass-lifecycle test-metaclass-construction test-untyped-pointer-dereference test-ansistring-pointer-cast test-old-file-api test-named-text-input test-file-lifecycle test-file-copy-rejected test-getdir test-rmdir test-environment-variable test-get-local-time test-file-date-to-date-time test-decode-date-time test-tdatetime test-change-file-ext test-execute-process test-unix-fpsystem test-try-statements test-exception-handlers test-integer-not test-shift-operators test-pointer-equality test-sysutils-exception test-findfirst-findnext test-old-object-lifecycle test-incomplete-type-resolution test-final-methods test-abstract-methods test-abstract-classes test-class-static-methods test-interface-guid test-interfaces-directive test-type-identity-and-compatibility test-operator-argument-matching test-operator-ranking-policies test-overload-ranking test-operator-catalog test-managed-types-and-iteration test-overflow-checking test-q-intrinsics test-q-directive-scoping test-inc-dec-operators test-custom-checked-operators test-custom-range-conversions test-custom-explicit-conversions test-overflow-constants test-index-range-checking test-subrange-carriers test-implicit-ordinal-narrowing test-based-integer-literals test-implicit-conversion-single-edge test-implicit-real-narrowing test-real-constant-semantics
.PHONY: test-strtoint
.PHONY: test-inttostr
.PHONY: test-strpas
.PHONY: test-delete-file
.PHONY: test-file-error-state
.PHONY: test-omitted-out-byte-pointer
.PHONY: test-swapendian-builtin
.PHONY: test-symbolic-operator-rejection
.PHONY: test-predefined-explicit-conversions
.PHONY: test-class-pointer-conversion
.PHONY: test-custom-in-operator
.PHONY: test-emission-semantic-boundaries
.PHONY: test-function-result-designator
.PHONY: test-pointer-arithmetic
.PHONY: test-set-arithmetic
.PHONY: test-diagnostic-context
.PHONY: test-frame-intrinsics
