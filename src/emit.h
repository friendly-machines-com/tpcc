#pragma once
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

class Node;
class Type;
class EnumType;
class PackedRecordType;
class Callable;
class Method;
class RoutineType;
class RoutineRef;
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
	void emit_class_constructor_call(Method* method);
	// Emit a C++ struct/class definition for a named record/class/object
	// type. Fields and method prototypes go inside; method bodies are still
	// emitted separately (outside the class) by emit_procedure_open.
	void emit_type_definition(std::string cxx_name, Type* ty);
	// Emit `using <cxx_name> = <aliased_cxx_name>;` for `type B = A;` where A
	// is an already-named aggregate/enum. Avoids re-emitting A's body under B's
	// name (ODR violation in C++).
	void emit_type_alias(
	    std::string cxx_name, Type* aliased_type);
	void emit_var_decl(std::string cxx_name, Type* ty);
	void emit_const_decl(std::string cxx_name, Type* ty, Node* initializer);
	void emit_main_prologue(
	    const std::vector<UnitLifecycleNames>&
	        unit_lifecycle_hooks);
	void emit_main_epilogue();
	void emit_statement(Node* stmt);
	void emit_label(std::string cxx_label_name);
	void emit_goto(std::string cxx_label_name);
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
	void emit_for_prologue(Node* control, Node* initial, Node* final, bool descending);
	void emit_for_epilogue();
	void emit_loop_control(bool is_break);

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
	// which wrapper the caller invokes).
	void emit_routine_signature(RoutineType* ty, std::string cxx_text, Position pos, std::string owner_qualifier);
	// emit_callable_signature is a thin wrapper for callers that hold a
	// Callable*. Forwards (c->ty, callable_cxx_name(c), pos, owner_qualifier)
	// to emit_routine_signature.
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
	// The parameter-type spelling is shared by declarations, routine-value
	// types, and method-adapter pointer-to-member casts. `with_name` controls
	// only whether the Pascal formal's generated C++ identifier follows it.
	void emit_formal_parameter(const Parameter& formal, bool with_name);
	void emit_formal_parameters(RoutineType* ty, bool with_names);
	// Emit the function type `Result(Args...)` (not a pointer and not a
	// declaration). m_proc and m_method both take this as their template
	// argument.
	void emit_function_type(RoutineType* ty);
	// Emit a full enum declaration body: `enum [NAME] { a, b, c }` -- no
	// leading newline, no trailing semicolon. Caller frames those. Used by
	// emit_type_definition (named, at type-block scope) and emit_type_ref's
	// anonymous-enum branch (inline `var x: (A, B, C);`). References to an
	// already-defined named enum do NOT go through here -- those just
	// spell the cxx name.
	void emit_enum_decl(EnumType* e);
	// Emit the opaque byte carrier and generated direct-field accessors for a
	// PackedRecordType. Packed records deliberately do not flow through
	// emit_aggregate_decl because they have no C++ field members.
	void emit_packed_record_decl(std::string cxx_name, PackedRecordType* p);
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
