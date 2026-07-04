#pragma once
#include <cstdio>
#include <string>

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
private:
	FILE* out;
	int fresh_counter;
public:
	Emitter();
	~Emitter();
	void open_for_program(std::string output_path);
	void close();

	/** Generate a fresh identifier of the form `<prefix>_N`, unique per
	 *  Emitter. Callers pick a prefix that stays clear of C++'s reserved
	 *  name rules (i.e. no leading underscore); `pas_` is the convention. */
	std::string next_fresh_cxx_name(std::string prefix);

	void emit_program_prologue(std::string program_name);
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
	void emit_procedure_close();

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
	void emit_aggregate_decl(std::string cxx_name, Type* ty);
};
