#pragma once
#include <cstdio>
#include <string>
#include <vector>

class Node;
class Type;
class EnumType;
class Callable;

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

private:
	FILE* out_h;       // null for programs
	FILE* out_cc;
	FILE* active;      // points at out_h or out_cc; null until a section is set
	int fresh_counter;

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

	/** Generate a fresh identifier of the form `<prefix>_N`, unique per
	 *  Emitter. Callers pick a prefix that stays clear of C++'s reserved
	 *  name rules (i.e. no leading underscore); `pas_` is the convention. */
	std::string next_fresh_cxx_name(std::string prefix);

	void emit_program_prologue(std::vector<std::string> used_unit_h_files);
	// Emit a per-unit .h prologue: #include "rtl.h" plus an #include per
	// interface-section `uses`d unit, so consumers of this header see the
	// transitive types referenced by the interface declarations.
	void emit_unit_interface_prologue(std::vector<std::string> used_unit_h_files);
	// Emit a per-unit .cc prologue: #include "<this_unit>.h" (the unit's own
	// interface section) plus #include "rtl.h" and per-impl-`uses`d-unit
	// #includes.
	void emit_unit_implementation_prologue(std::string this_unit_h_file, std::vector<std::string> impl_used_unit_h_files);
	// Emit a C++ struct/class definition for a named record/class/object
	// type. Fields and method prototypes go inside; method bodies are still
	// emitted separately (outside the class) by emit_procedure_open.
	void emit_type_definition(std::string cxx_name, Type* ty);
	// Emit `using <cxx_name> = <aliased_cxx_name>;` for `type B = A;` where A
	// is an already-named aggregate/enum. Avoids re-emitting A's body under B's
	// name (ODR violation in C++).
	void emit_type_alias(std::string cxx_name, std::string aliased_cxx_name);
	void emit_var_decl(std::string cxx_name, Type* ty);
	void emit_main_prologue();
	void emit_main_epilogue();
	void emit_statement(Node* stmt);
	void emit_with_prologue(std::string alias_cxx_name, Node* target);
	void emit_with_epilogue();
	// Control-flow framing. Each is parse-time emission: parser parses the
	// condition/body via its usual recursive parse_statement / parse_block_body
	// calls (which themselves emit), and these methods wrap that output in
	// the matching C++ construct.
	void emit_if_prologue(Node* condition);
	void emit_if_else();
	void emit_if_epilogue();
	void emit_while_prologue(Node* condition);
	void emit_while_epilogue();
	void emit_repeat_prologue();
	void emit_repeat_epilogue(Node* condition);

	// Procedure/function definition emission. emit_procedure_open writes the
	// C++ signature plus opening brace; body statements emit between; then
	// emit_procedure_close writes the closing brace.
	void emit_procedure_open(Callable* c);
	void emit_procedure_close(Callable* c);

	void emit_expression(Node* expr);
	void emit_type_ref(Type* ty);

    private:
	// Emit a full enum declaration body: `enum [NAME] { a, b, c }` -- no
	// leading newline, no trailing semicolon. Caller frames those. Used by
	// emit_type_definition (named, at type-block scope) and emit_type_ref's
	// anonymous-enum branch (inline `var x: (A, B, C);`). References to an
	// already-defined named enum do NOT go through here -- those just
	// spell the cxx name.
	void emit_enum_decl(EnumType* e);
	// Emit a record/class/object body: `<kw> [NAME] { <fields> <variant-union> }`
	// -- no leading newline, no trailing semicolon. Caller frames those. Used
	// by emit_type_definition (named, top-level) and emit_type_ref's anonymous
	// branch (inline at use site, e.g. `var x: record ... end;`). A named
	// reference to an already-defined type does NOT go through here -- it just
	// spells the cxx name.
	void emit_aggregate_decl(std::string cxx_name, Type* ty, bool in_meta = false);
};
