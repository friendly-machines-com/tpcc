#pragma once
#include <cstdio>
#include <string>

class Node;
class Type;

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
public:
	Emitter();
	~Emitter();
	void open_for_program(std::string output_path);
	void close();

	void emit_program_prologue(std::string program_name);
	void emit_var_decl(std::string cxx_name, Type* ty);
	void emit_main_prologue();
	void emit_main_epilogue();
	void emit_statement(Node* stmt);

	void emit_expression(Node* expr);
	void emit_type_ref(Type* ty);
};
