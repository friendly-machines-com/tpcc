#include "emit.h"
#include "cst.h"
#include "types.h"
#include "builtins.h"

std::string pascal_to_cxx_name(std::string pascal_name) {
	// Identity for now. Future work:
	//   - mangle C++ reserved words that are legal Pascal identifiers
	//     (class, template, new, delete, this, virtual, ...).
	//   - prefix with unit name once cross-unit references need disambiguation.
	return pascal_name;
}

Emitter::Emitter() : out(nullptr) {}

Emitter::~Emitter() {
	close();
}

void Emitter::open_for_program(std::string output_path) {
	out = fopen(output_path.c_str(), "w");
}

void Emitter::close() {
	if (out) {
		fclose(out);
		out = nullptr;
	}
}

void Emitter::emit_program_prologue(std::string program_name) {
	if (!out) return;
	fprintf(out, "#include \"rtl/rtl.h\"\n\n");
}

void Emitter::emit_var_decl(std::string cxx_name, Type* ty) {
	if (!out) return;
	emit_type_ref(ty);
	fprintf(out, " %s;\n", cxx_name.c_str());
}

void Emitter::emit_main_prologue() {
	if (!out) return;
	fprintf(out, "\nint main() {\n");
}

void Emitter::emit_main_epilogue() {
	if (!out) return;
	fprintf(out, "\treturn 0;\n}\n");
}

void Emitter::emit_statement(Node* stmt) {
	if (!out) return;
	if (auto a = dynamic_cast<Assign*>(stmt)) {
		fprintf(out, "\t");
		emit_expression(a->a);
		fprintf(out, " = ");
		emit_expression(a->b);
		fprintf(out, ";\n");
		return;
	}
	fprintf(out, "\t/* unsupported statement */\n");
}

void Emitter::emit_expression(Node* expr) {
	if (!out) return;
	if (auto c = dynamic_cast<Constant*>(expr)) {
		fprintf(out, "%llu", (unsigned long long)c->value);
		return;
	}
	if (auto s = dynamic_cast<StorageSlot*>(expr)) {
		fprintf(out, "%s", s->cxx_name.c_str());
		return;
	}
	fprintf(out, "/* unsupported expression */");
}

void Emitter::emit_type_ref(Type* ty) {
	if (!out) return;
	if (auto it = dynamic_cast<IntrinsicType*>(ty)) {
		fprintf(out, "%.*s", (int)it->rtl_name.size(), it->rtl_name.data());
		return;
	}
	fprintf(out, "/* unsupported type */");
}
