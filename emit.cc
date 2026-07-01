#include <cstdlib>
#include <typeinfo>
#include "emit.h"
#include "cst.h"
#include "types.h"
#include "builtins.h"

// NODE may be null; SITE names the caller for the error message.
[[noreturn]] static void unhandled_node(const char* site, const Node* node) {
	if (node) {
		fprintf(stderr, "internal compiler error: %s does not handle node kind '%s'\n",
		        site, typeid(*node).name());
	} else {
		fprintf(stderr, "internal compiler error: %s called with null node\n", site);
	}
	fflush(stderr);
	exit(1);
}
[[noreturn]] static void unhandled_type(const char* site, const Type* ty) {
	if (ty) {
		fprintf(stderr, "internal compiler error: %s does not handle type kind '%s'\n",
		        site, typeid(*ty).name());
	} else {
		fprintf(stderr, "internal compiler error: %s called with null type\n", site);
	}
	fflush(stderr);
	exit(1);
}

std::string pascal_to_cxx_name(std::string pascal_name) {
	// Identity for now. Future work:
	//   - mangle C++ reserved words that are legal Pascal identifiers
	//     (class, template, new, delete, this, virtual, ...).
	//   - prefix with unit name once cross-unit references need disambiguation.
	return pascal_name;
}

Emitter::Emitter() : out(nullptr), fresh_counter(0) {}

std::string Emitter::next_fresh_cxx_name(std::string prefix) {
	return prefix + "_" + std::to_string(++fresh_counter);
}

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
	unhandled_node("emit_statement", stmt);
}

void Emitter::emit_with_prologue(std::string alias_cxx_name, Node* target) {
	if (!out) return;
	// `auto&&` binds an lvalue target as a reference and lifetime-extends an
	// rvalue target (e.g. a function call returning a record by value), so the
	// with-body sees a single evaluation of the target expression regardless
	// of value category.
	fprintf(out, "\t{ auto&& %s = ", alias_cxx_name.c_str());
	emit_expression(target);
	fprintf(out, ";\n");
}

void Emitter::emit_with_epilogue() {
	if (!out) return;
	fprintf(out, "\t}\n");
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
	if (auto m = dynamic_cast<MemberAccess*>(expr)) {
		emit_expression(m->a);
		fprintf(out, ".");
		emit_expression(m->b);
		return;
	}
	unhandled_node("emit_expression", expr);
}

void Emitter::emit_type_ref(Type* ty) {
	if (!out) return;
	if (auto it = dynamic_cast<IntrinsicType*>(ty)) {
		fprintf(out, "%.*s", (int)it->rtl_name.size(), it->rtl_name.data());
		return;
	}
	unhandled_type("emit_type_ref", ty);
}
