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
	if (auto pc = dynamic_cast<ProcCall*>(stmt)) {
		fprintf(out, "\t");
		emit_expression(pc);
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

void Emitter::emit_procedure_open(Callable* c) {
	if (!out) return;
	fprintf(out, "\n");
	emit_type_ref(c->return_type);
	fprintf(out, " %s(", c->cxx_name.c_str());
	for (size_t i = 0; i < c->formals.size(); i++) {
		if (i > 0) fprintf(out, ", ");
		auto& f = c->formals[i];
		if (f.mode == ParamMode::Const) fprintf(out, "const ");
		emit_type_ref(f.ty);
		if (f.mode == ParamMode::Var || f.mode == ParamMode::Out
		    || f.mode == ParamMode::Const) {
			fprintf(out, "&");
		}
		fprintf(out, " %s", f.cxx_name.c_str());
	}
	fprintf(out, ") {\n");
}

void Emitter::emit_procedure_close() {
	if (!out) return;
	fprintf(out, "}\n");
}

// Result-type of the whole expression drives the choice between bitwise and
// short-circuit for `and`/`or`; other ops have a unique mapping.
static const char* cxx_binary_operator(BinaryOperation* op) {
	bool booleans = op->ty == boolean_type();
	if (dynamic_cast<Add*>(op))            return "+";
	if (dynamic_cast<Subtract*>(op))       return "-";
	if (dynamic_cast<Multiply*>(op))       return "*";
	if (dynamic_cast<Divide*>(op))         return "/";
	if (dynamic_cast<Div*>(op))            return "/";
	if (dynamic_cast<Mod*>(op))            return "%";
	if (dynamic_cast<And*>(op))            return booleans ? "&&" : "&";
	if (dynamic_cast<Or*>(op))             return booleans ? "||" : "|";
	if (dynamic_cast<Xor*>(op))            return "^";
	if (dynamic_cast<ShiftLeft*>(op))      return "<<";
	if (dynamic_cast<ShiftRight*>(op))     return ">>";
	if (dynamic_cast<Equal*>(op))          return "==";
	if (dynamic_cast<NotEqual*>(op))       return "!=";
	if (dynamic_cast<Less*>(op))           return "<";
	if (dynamic_cast<Greater*>(op))        return ">";
	if (dynamic_cast<LessOrEqual*>(op))    return "<=";
	if (dynamic_cast<GreaterOrEqual*>(op)) return ">=";
	return nullptr;
}

static const char* cxx_unary_operator(UnaryOperation* op) {
	if (dynamic_cast<Not*>(op))         return "!";
	if (dynamic_cast<Negate*>(op))      return "-";
	if (dynamic_cast<Positivize*>(op))  return "+";
	if (dynamic_cast<AddrOf*>(op))      return "&";
	if (dynamic_cast<Dereference*>(op)) return "*";
	return nullptr;
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
	if (auto b = dynamic_cast<Builtin*>(expr)) {
		fprintf(out, "%.*s", (int)b->desc->rtl_name.size(), b->desc->rtl_name.data());
		return;
	}
	if (auto c = dynamic_cast<Callable*>(expr)) {
		fprintf(out, "%s", c->cxx_name.c_str());
		return;
	}
	if (auto m = dynamic_cast<MemberAccess*>(expr)) {
		emit_expression(m->a);
		fprintf(out, ".");
		emit_expression(m->b);
		return;
	}
	if (auto pc = dynamic_cast<ProcCall*>(expr)) {
		if (pc->receiver) {
			emit_expression(pc->receiver);
			// `->` when receiver's static type is a Pascal pointer; `.` otherwise.
			bool ptr = pc->receiver->ty && dynamic_cast<PointerType*>(pc->receiver->ty);
			fprintf(out, "%s", ptr ? "->" : ".");
		}
		emit_expression(pc->callee);
		fprintf(out, "(");
		for (size_t i = 0; i < pc->args.size(); i++) {
			if (i > 0) fprintf(out, ", ");
			emit_expression(pc->args[i]);
		}
		fprintf(out, ")");
		return;
	}
	if (auto ca = dynamic_cast<Cast*>(expr)) {
		fprintf(out, "static_cast<");
		emit_type_ref(ca->ty);
		fprintf(out, ">(");
		emit_expression(ca->a);
		fprintf(out, ")");
		return;
	}
	if (auto co = dynamic_cast<Coerce*>(expr)) {
		fprintf(out, "static_cast<");
		emit_type_ref(co->ty);
		fprintf(out, ">(");
		emit_expression(co->a);
		fprintf(out, ")");
		return;
	}
	if (auto u = dynamic_cast<UnaryOperation*>(expr)) {
		if (const char* op = cxx_unary_operator(u)) {
			fprintf(out, "%s", op);
			emit_expression(u->a);
			return;
		}
	}
	if (auto bin = dynamic_cast<BinaryOperation*>(expr)) {
		if (const char* op = cxx_binary_operator(bin)) {
			fprintf(out, "(");
			emit_expression(bin->a);
			fprintf(out, " %s ", op);
			emit_expression(bin->b);
			fprintf(out, ")");
			return;
		}
	}
	unhandled_node("emit_expression", expr);
}

void Emitter::emit_type_ref(Type* ty) {
	if (!out) return;
	// Follow IncompleteType placeholders through to the real underlying type.
	while (auto inc = dynamic_cast<IncompleteType*>(ty)) {
		if (!inc->resolved) break;
		ty = inc->resolved;
	}
	if (auto it = dynamic_cast<IntrinsicType*>(ty)) {
		fprintf(out, "%.*s", (int)it->rtl_name.size(), it->rtl_name.data());
		return;
	}
	if (dynamic_cast<UnitType*>(ty)) {
		fprintf(out, "void");
		return;
	}
	// TODO: emit struct definitions at type-block time and then just spell
	// the name here. For now, spell the name if we have one; leave a comment
	// where struct emission has to land.
	if (auto r = dynamic_cast<RecordType*>(ty)) {
		fprintf(out, "%s /*record*/", r->cxx_name.empty() ? "struct{}" : r->cxx_name.c_str());
		return;
	}
	if (auto c = dynamic_cast<ClassType*>(ty)) {
		fprintf(out, "%s /*class*/", c->cxx_name.empty() ? "struct{}" : c->cxx_name.c_str());
		return;
	}
	if (auto o = dynamic_cast<ObjectType*>(ty)) {
		fprintf(out, "%s /*object*/", o->cxx_name.empty() ? "struct{}" : o->cxx_name.c_str());
		return;
	}
	if (auto p = dynamic_cast<PointerType*>(ty)) {
		emit_type_ref(p->item_type);
		fprintf(out, "*");
		return;
	}
	unhandled_type("emit_type_ref", ty);
}
