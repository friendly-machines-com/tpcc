#pragma once
#include <cstdio>
#include <string>

class Node;
class Type;
class Procedure;

/** Translate a Pascal source identifier to the identifier that will be written
 *  into the emitted C++ output. Currently identity; the extension point for
 *  mangling (C++ reserved words like `class`, `template`, `new`; later,
 *  unit-name prefixing so cross-unit references don't collide). Kept as a
 *  free function because it's stateless and callers (StorageSlot ctor sites)
 *  don't need an Emitter instance yet. */
std::string pascal_to_cxx_name(std::string pascal_name);

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
	void emit_var_decl(std::string cxx_name, Type* ty);
	void emit_main_prologue();
	void emit_main_epilogue();
	void emit_statement(Node* stmt);
	void emit_with_prologue(std::string alias_cxx_name, Node* target);
	void emit_with_epilogue();

	// Procedure/function definition emission. emit_procedure_open writes the
	// C++ signature plus opening brace; body statements emit between; then
	// emit_procedure_close writes the closing brace.
	void emit_procedure_open(Procedure* p);
	void emit_procedure_close();

	void emit_expression(Node* expr);
	void emit_type_ref(Type* ty);
};
