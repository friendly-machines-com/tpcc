#pragma once
#include <cstdio>
#include <set>
#include <string>
#include <utility>
#include <vector>

class Node;
class Type;
class EnumType;
class SubrangeType;
class PackedRecordType;
struct VariantPart;
class Callable;
class Method;
class RoutineType;
class RoutineRef;
class StorageSlot;
struct Parameter;

struct UnitLifecycleNames {
	std::string cxx_namespace;
	std::string initialize;
	std::string finalize;
};

/** No public name-mangling entry point. Prefixing (`t_` for type identifiers,
 *  `p_` for value identifiers) is applied inside Type / Node constructors
 *  where the kind is implicit -- callers pass Pascal names and the ctor
 *  decides the prefix. This keeps C++ reserved-word collisions (`class`,
 *  `new`, `false`, ...) from leaking to every emission call site.
 *
 *  Exceptions: `cxx_value_name` and `cxx_type_name` are the explicit-prefix
 *  helpers for the (rare) sites that construct a cxx identifier from a Pascal
 *  name without going through a node ctor (e.g. naming the canonical cxx
 *  identifier to store on a Type at the `type X = ...` alias site, since
 *  types don't take their name in the ctor). They apply the same `p_` / `t_`
 *  prefix the value-ctor path applies internally. */
std::string cxx_value_name(std::string pas_name);
std::string cxx_type_name(std::string pas_name);

class Emitter {
public:
	// Which output target subsequent emit_* calls land in. Programs only ever
	// use Implementation (their single .cc); units flip to Header while
	// emitting the interface section and back to Implementation for the
	// implementation section.
	enum class Section { Header, Implementation };

	// Whether emit_routine_signature is emitting a prototype (Declaration),
	// a body-opener signature (Definition), or just `(formals)`
	// (DeclarationFormalsOnly -- used by lambda parameter lists). Required
	// at every call site so the call site documents its intent;
	// DeclarationFormalsOnly short-circuits before any return-type/name/
	// qualifier emission.
	enum class Position { Declaration, Definition, DeclarationFormalsOnly };

private:
	FILE* out_h;       // null for programs
	FILE* out_cc;
	FILE* active;      // points at out_h or out_cc; null until a section is set
	std::string active_unit_namespace;
	std::set<SubrangeType*> emitted_subranges;
	void emit_return_transfer_handler(
	    unsigned try_depth, RoutineType* routine);

public:
	Emitter();
	~Emitter();
	// Open just <output_path> for a program (single .cc, no header). Sets
	// active to the .cc stream so subsequent emit_* calls land there.
	void open_for_program(std::string output_path);
	// Open <output_dir>/<unit_name>.h and .cc. Sets active to the .h stream
	// (units start their parse in the interface section).
	void open_for_unit(std::string unit_name, std::string output_dir);
	void close();
	bool is_open() const { return out_cc != nullptr; }
	void set_section(Section s);

	void emit_program_prologue(std::vector<std::string> used_unit_h_files);
	// Emit a per-unit .h prologue: #include "rtl.h" plus an #include per
	// interface-section `uses`d unit, so consumers of this header see the
	// transitive types referenced by the interface declarations.
	void emit_unit_interface_prologue(
	    std::string unit_namespace,
	    std::vector<std::string> used_unit_h_files);
	void emit_unit_interface_epilogue();
	// Emit a per-unit .cc prologue: #include "<this_unit>.h" (the unit's own
	// interface section) plus #include "rtl.h" and per-impl-`uses`d-unit
	// #includes.
	void emit_unit_implementation_prologue(
	    std::string unit_namespace,
	    std::string this_unit_h_file,
	    std::vector<std::string> impl_used_unit_h_files);
	void emit_unit_implementation_epilogue();
	void emit_unit_lifecycle_open(std::string cxx_name);
	void emit_unit_lifecycle_close();
	void emit_class_lifecycle_call(Method* method);
	// Emit a C++ struct/class definition for a named record/class/object
	// type. Fields and method prototypes go inside; method bodies are still
	// emitted separately (outside the class) by emit_procedure_open.
	void emit_type_definition(std::string cxx_name, Type* ty);
	void emit_class_forward_declaration(
	    std::string cxx_name);
	// Emit `using <cxx_name> = <aliased_cxx_name>;` for `type B = A;` where A
	// is an already-named aggregate/enum. Avoids re-emitting A's body under B's
	// name (ODR violation in C++).
	void emit_type_alias(
	    std::string cxx_name, Type* aliased_type);
	void emit_var_decl(
	    std::string cxx_name, Type* ty,
	    Node* initializer = nullptr);
	void emit_initialized_storage_decl(
	    std::string cxx_name, Type* ty,
	    Node* initializer, bool routine_local);
	void emit_main_prologue(
	    const std::vector<UnitLifecycleNames>&
	        unit_lifecycle_hooks,
	    const std::vector<Method*>&
	        program_class_destructors);
	void emit_program_finalizer_registration();
	void emit_main_epilogue(
	    bool has_program_class_destructors);
	void emit_statement(Node* stmt);
	void emit_label(std::string cxx_label_name);
	void emit_goto(std::string cxx_label_name);
	// The common prefix streams a protected Pascal try body immediately.
	// The suffix then selects except or finally, while the outer private
	// catches propagate or realize Exit/break/continue after every crossed
	// Pascal try has performed its work.
	void emit_try_prologue();
	void emit_try_except_prologue();
	void emit_exception_handler_prologue(
	    Type* exception_type, std::string variable_cxx_name,
	    bool first);
	void emit_exception_handler_epilogue();
	void emit_exception_default_prologue();
	void emit_exception_default_epilogue();
	void emit_try_except_epilogue(
	    bool typed_handlers, bool has_default);
	void emit_try_finally_prologue();
	void emit_try_finally_epilogue();
	void emit_try_control_epilogue(
	    unsigned try_depth, RoutineType* routine,
	    bool inside_loop);
	// Close a generated cleanup region which contains its loop. A break that
	// reaches this boundary is already complete; emitting a C++ break here
	// would be outside the loop.
	void emit_for_in_cleanup_control_epilogue(
	    unsigned try_depth, RoutineType* routine);
	void emit_with_prologue(std::string alias_cxx_name, Node* target);
	void emit_with_epilogue();
	// Control-flow framing. Each is parse-time emission: parser parses the
	// condition/body via its usual recursive parse_statement / parse_block_body
	// calls (which themselves emit), and these methods wrap that output in
	// the matching C++ construct.
	void emit_if_prologue(Node* condition);
	void emit_if_else();
	void emit_if_epilogue();
	// Pascal case is emitted as an if/else-if chain rather than C++ switch:
	// labels may be non-integral and ranges become two comparisons. The
	// selector prologue snapshots the expression exactly once.
	void emit_case_prologue(std::string selector_cxx_name, Node* selector);
	void emit_case_arm_prologue(Node* condition, bool first);
	void emit_case_arm_epilogue();
	void emit_case_else_prologue(bool has_previous_arm);
	void emit_case_epilogue();
	void emit_while_prologue(Node* condition);
	void emit_while_epilogue();
	void emit_repeat_prologue();
	void emit_repeat_epilogue(Node* condition);
	void emit_for_prologue(
	    Node* control, Node* initial,
	    Node* final, bool descending,
	    bool overflow_checks);
	void emit_for_epilogue();
	void emit_for_in_sequence_prologue(
	    Node* collection, Node* current_assignment);
	void emit_for_in_set_prologue(
	    Node* collection, Node* lower,
	    Node* upper, Node* current_assignment);
	void emit_for_in_ordinal_prologue(
	    Type* ordinal_type, Node* lower,
	    Node* upper, Node* current_assignment);
	void emit_for_in_custom_setup(
	    Node* get_enumerator, bool nullable);
	void emit_for_in_custom_loop_prologue(
	    Node* move_next,
	    Node* current_assignment);
	void emit_for_in_loop_epilogue();
	void emit_for_in_custom_epilogue(
	    bool nullable);
	void emit_for_in_epilogue();
	void emit_loop_control(
	    bool is_break, unsigned try_depth = 0,
	    unsigned target_try_depth = 0);

	// Procedure/function definition emission. emit_procedure_open writes the
	// C++ signature plus opening brace; body statements emit between; then
	// emit_procedure_close writes the closing brace.
	void emit_procedure_open(Callable* c, bool nested_lambda = false);
	void emit_procedure_close(Callable* c, bool nested_lambda = false);

	// emit_routine_signature is THE primitive emitter for a routine
	// signature. Emits `Ret [qual]cxx_text(formals)` (or just `(formals)`
	// when pos == DeclarationFormalsOnly). No leading whitespace, no
	// terminator, no decorations. The formals loop lives here and nowhere
	// else.
	//
	// cxx_text carries the C++ spelling of the token that sits between
	// return-type and `(formals)`: a real cxx_name like `p_foo` or `~t_foo`
	// for callable cases, or empty for DeclarationFormalsOnly.
	// owner_qualifier is `Foo::` or empty (namespace only -- orthogonal to
	// prototype-vs-definition, which is expressed at the function level via
	// which wrapper the caller invokes). conversion_operator_target is null
	// for every ordinary routine; otherwise it appends the backend-only
	// destination tag which makes a Pascal contextual conversion representable
	// in the C++ overload set.
	void emit_routine_signature(
	    RoutineType* ty, std::string cxx_text,
	    Position pos, std::string owner_qualifier,
	    bool cxx_destructor,
	    Type* conversion_operator_target);
	// emit_callable_signature is a thin wrapper for callers that hold a
	// Callable*. It also derives the conversion-operator target tag from the
	// callable category; ordinary callers of emit_routine_signature pass null.
	void emit_callable_signature(Callable* c, Position position, std::string owner_qualifier);
	// emit_callable_prototype emits `<prefix><sig><suffix>;\n`. THE prototype
	// emitter for every case: standalone procedure prototypes, in-class method
	// prototypes, interface prototypes, m_meta prototypes. Caller passes any
	// leading whitespace as part of `prefix` ("\n" for top-level, "\t" for
	// in-class, optionally combined with `virtual ` etc.); the parser never
	// touches the emitter's `active` directly.
	void emit_callable_prototype(Callable* c, std::string owner_qualifier, std::string prefix, std::string suffix);

	void emit_expression(Node* expr);
	void emit_type_ref(Type* ty);

    private:
	// Emit every concrete subrange carrier whose spelling is required by TY
	// before beginning the surrounding C++ declaration. INSPECT_DEFINITION is
	// true only when that surrounding declaration owns aggregate members;
	// references to an already-defined named aggregate need only its tag.
	void emit_type_dependencies(
	    Type* ty, bool inspect_definition = false);
	void emit_subrange_definition(
	    SubrangeType* subrange);
	// The parameter-type spelling is shared by declarations, routine-value
	// types, and method-adapter pointer-to-member casts. `with_name` controls
	// only whether the Pascal formal's generated C++ identifier follows it.
	// A non-null conversion_operator_target appends an unnamed C++ tag after
	// all Pascal-visible formals.
	void emit_formal_parameter(const Parameter& formal, bool with_name);
	void emit_formal_parameters(
	    RoutineType* ty, bool with_names,
	    Type* conversion_operator_target);
	// Emit the function type `Result(Args...)` (not a pointer and not a
	// declaration). m_proc and m_method both take this as their template
	// argument.
	void emit_function_type(RoutineType* ty);
	void emit_call_arguments(
	    RoutineType* ty, const std::vector<Node*>& args);
	// Emit a full enum declaration body: `enum [NAME] { a, b, c }` -- no
	// leading newline, no trailing semicolon. Caller frames those. Used by
	// emit_type_definition (named, at type-block scope) and emit_type_ref's
	// anonymous-enum branch (inline `var x: (A, B, C);`). References to an
	// already-defined named enum do NOT go through here -- those just
	// spell the cxx name.
	void emit_enum_decl(EnumType* e);
	// Emit storage owned by an aggregate rather than by each instance.
	// Initialized storage uses a function-local static behind an inline
	// accessor, which is valid even when the Pascal aggregate is lowered to a
	// C++ local class (local classes cannot have static data members).
	void emit_static_member_declaration(
	    StorageSlot* slot);
	// Emit the opaque byte carrier and generated direct-field accessors for a
	// PackedRecordType. Packed records deliberately do not flow through
	// emit_aggregate_decl because they have no C++ field members.
	void emit_packed_record_decl(std::string cxx_name, PackedRecordType* p);
	// Emit one ordinary-record variant part. Nested Pascal variant parts are
	// recursively nested in the corresponding C++ arm structure, preserving
	// both sequential fields within an arm and overlap between arms.
	void emit_record_variant_decl(
	    VariantPart* variant, unsigned indent,
	    unsigned depth);
	// Emit a record/class/object body: `<kw> [NAME] { <fields> <variant-union> }`
	// -- no leading newline, no trailing semicolon. Caller frames those. Used
	// by emit_type_definition (named, top-level) and emit_type_ref's anonymous
	// branch (inline at use site, e.g. `var x: record ... end;`). A named
	// reference to an already-defined type does NOT go through here -- it just
	// spells the cxx name.
	void emit_aggregate_decl(std::string cxx_name, Type* ty, bool in_meta = false);

	// Emit a resolved @routine value. Global routines become ordinary function
	// pointers; methods use the common m_bind_method pointer-to-member template
	// adapter and retain their receiver in the Data word.
	void emit_routine_reference(RoutineRef* reference);
	// Emit an expression in a context which may mutate the referenced place
	// (var/out, address-of). Compiler-synthesized properties can use a
	// different accessor here, for example AnsiString's uniqueness barrier.
	void emit_writable_expression(Node* expr);
	// Untyped var/out parameters receive a bounded view of the underlying
	// Pascal storage rather than a C++ reference to only the selected
	// subobject. Built-in indexed properties preserve their container and
	// index here so the RTL can compute the remaining extent safely.
	void emit_storage_ref(Node* expr);
	void emit_const_storage_ref(Node* expr);
	void emit_template_value_arg(Node* expr);
};
