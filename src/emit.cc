#include "emit.h"
#include "builtins.h"
#include "cst.h"
#include "frame.h"
#include "types.h"
#include <cstdlib>
#include <set>
#include <typeinfo>

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

void Emitter::emit_enum_decl(EnumType* e) {
	if (!out)
		return;
	fprintf(out, "enum ");
	if (!e->cxx_name.empty())
		fprintf(out, "%s ", e->cxx_name.c_str());
	fprintf(out, "{ ");
	for (size_t i = 0; i < e->members.size(); i++) {
		if (i)
			fprintf(out, ", ");
		fprintf(out, "%s", e->members[i].cxx_name.c_str());
	}
	fprintf(out, " }");
}

// Apply the `p_` prefix to a Pascal value identifier. Used at sites that
// construct a cxx identifier without going through a value-node ctor (rare):
// e.g. naming the slot for a record variant selector, naming an enum member
// at parse time, etc. The prefix stays clear of C++ reserved words that are
// legal Pascal identifiers (`new`, `class`, `false`, ...).
std::string cxx_value_name(std::string pas_name) {
	return "p_" + pas_name;
}

// Apply the `t_` prefix to a Pascal type identifier. Used at the
// `type X = ...` alias site (parse_type_block) where the canonical cxx name
// is assigned to a Type -- types don't take their name in their ctor (a Type
// is nameless; names are Frame bindings, plural/optional), so the prefix
// can't be applied inside the Type ctor the way it can for value nodes.
std::string cxx_type_name(std::string pas_name) {
	return "t_" + pas_name;
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
	if (!out)
		return;
	fprintf(out, "#include \"rtl.h\"\n\n");
}

void Emitter::emit_var_decl(std::string cxx_name, Type* ty) {
	if (!out)
		return;
	emit_type_ref(ty);
	fprintf(out, " %s;\n", cxx_name.c_str());
}

void Emitter::emit_main_prologue() {
	if (!out)
		return;
	fprintf(out, "\nint main() {\n");
}

void Emitter::emit_main_epilogue() {
	if (!out)
		return;
	fprintf(out, "\treturn 0;\n}\n");
}

void Emitter::emit_statement(Node* stmt) {
	if (!out)
		return;
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
	if (auto r = dynamic_cast<Return*>(stmt)) {
		fprintf(out, "\treturn ");
		emit_expression(r->a);
		fprintf(out, ";\n");
		return;
	}
	unhandled_node("emit_statement", stmt);
}

void Emitter::emit_with_prologue(std::string alias_cxx_name, Node* target) {
	if (!out)
		return;
	// `auto&&` binds an lvalue target as a reference and lifetime-extends an
	// rvalue target (e.g. a function call returning a record by value), so the
	// with-body sees a single evaluation of the target expression regardless
	// of value category.
	fprintf(out, "\t{ auto&& %s = ", alias_cxx_name.c_str());
	emit_expression(target);
	fprintf(out, ";\n");
}

void Emitter::emit_with_epilogue() {
	if (!out)
		return;
	fprintf(out, "\t}\n");
}

void Emitter::emit_if_prologue(Node* condition) {
	if (!out)
		return;
	fprintf(out, "\tif (");
	emit_expression(condition);
	fprintf(out, ") {\n");
}

void Emitter::emit_if_else() {
	if (!out)
		return;
	fprintf(out, "\t} else {\n");
}

void Emitter::emit_if_epilogue() {
	if (!out)
		return;
	fprintf(out, "\t}\n");
}

void Emitter::emit_while_prologue(Node* condition) {
	if (!out)
		return;
	fprintf(out, "\twhile (");
	emit_expression(condition);
	fprintf(out, ") {\n");
}

void Emitter::emit_while_epilogue() {
	if (!out)
		return;
	fprintf(out, "\t}\n");
}

void Emitter::emit_repeat_prologue() {
	if (!out)
		return;
	fprintf(out, "\tdo {\n");
}

void Emitter::emit_repeat_epilogue(Node* condition) {
	if (!out)
		return;
	fprintf(out, "\t} while(!(");
	emit_expression(condition);
	fprintf(out, "));\n");
}

// The cxx_name of the type that owns a Method, or empty if none.
static std::string owner_cxx_name(Type* owner) {
	if (auto r = dynamic_cast<RecordType*>(owner))
		return r->cxx_name;
	if (auto c = dynamic_cast<ClassType*>(owner))
		return c->cxx_name;
	if (auto o = dynamic_cast<ObjectType*>(owner))
		return o->cxx_name;
	return "";
}

void Emitter::emit_procedure_open(Callable* c) {
	if (!out)
		return;
	fprintf(out, "\n");
	emit_type_ref(c->ty->return_type);
	fprintf(out, " ");
	if (auto m = dynamic_cast<Method*>(c)) {
		std::string owner = owner_cxx_name(m->owner_class);
		if (!owner.empty())
			fprintf(out, "%s::", owner.c_str());
	}
	fprintf(out, "%s(", c->cxx_name.c_str());
	for (size_t i = 0; i < c->ty->formals.size(); i++) {
		if (i > 0)
			fprintf(out, ", ");
		auto& f = c->ty->formals[i];
		if (f.mode == ParamMode::Const)
			fprintf(out, "const ");
		emit_type_ref(f.ty);
		if (f.mode == ParamMode::Var || f.mode == ParamMode::Out || f.mode == ParamMode::Const)
			fprintf(out, "&");
		fprintf(out, " %s", f.cxx_name.c_str());
	}
	fprintf(out, ") {\n");
}

void Emitter::emit_aggregate_decl(std::string cxx_name, Type* ty) {
	if (!out)
		return;
	Frame* body = nullptr;
	const char* kw = "struct";
	RecordType* rec = nullptr;
	if (auto r = dynamic_cast<RecordType*>(ty)) {
		body = r->children;
		rec = r;
		kw = "struct";
	} else if (auto c = dynamic_cast<ClassType*>(ty)) {
		body = c->children;
		kw = "struct";
	} else if (auto o = dynamic_cast<ObjectType*>(ty)) {
		body = o->children;
		kw = "struct";
	} else {
		unhandled_type("emit_aggregate_decl", ty);
	}
	const char* attributes = "";
	if (rec && rec->packed)
		attributes = "[[gnu::packed]] ";
	fprintf(out, "%s%s", attributes, kw);
	if (!cxx_name.empty())
		fprintf(out, " %s", cxx_name.c_str());
	fprintf(out, " {\n");
	// Variant-record emission strategy:
	//
	//   Pascal: a record is one flat namespace. Fixed fields, the optional
	//   selector, and every variant arm's fields all share the same scope.
	//   The arm structure exists ONLY to express that those slots overlap
	//   in memory -- it has no name-lookup or type-checking role.
	//
	//   C++ mapping: fixed fields and the optional selector both become
	//   ordinary struct members; the arms become ONE anonymous union whose
	//   members all alias (matching Pascal's overlap semantics).
	//
	//   Slot identity: variant slots are registered in the SAME Frame as
	//   fixed slots (so name lookup via body_frame_of sees them) AND in
	//   RecordType::arms (for source-order grouping). We skip them in the
	//   main walk by StorageSlot* identity.
	std::set<StorageSlot*> variant_slots;
	if (rec)
		for (auto& arm : rec->arms)
			for (auto& f : arm.fields)
				variant_slots.insert(f.slot);
	// TODO: Frame's std::map iterates alphabetically; Pascal semantics require
	// source order for layout. Preserve insertion order in a later pass.
	for (auto& kv : body->values()) {
		Node* v = kv.second.value;
		if (auto slot = dynamic_cast<StorageSlot*>(v)) {
			if (variant_slots.count(slot))
				continue;
			fprintf(out, "\t");
			emit_type_ref(kv.second.ty);
			fprintf(out, " %s;\n", slot->cxx_name.c_str());
		} else if (auto call = dynamic_cast<Callable*>(v)) {
			fprintf(out, "\t");
			if (auto m = dynamic_cast<Method*>(call)) {
				if (m->virtual_kind == Method::VirtualKind::Virtual || m->virtual_kind == Method::VirtualKind::Abstract || m->virtual_kind == Method::VirtualKind::Dynamic) {
					fprintf(out, "virtual ");
				}
			}
			emit_type_ref(call->ty->return_type);
			fprintf(out, " %s(", call->cxx_name.c_str());
			for (size_t i = 0; i < call->ty->formals.size(); i++) {
				if (i > 0)
					fprintf(out, ", ");
				auto& f = call->ty->formals[i];
				if (f.mode == ParamMode::Const)
					fprintf(out, "const ");
				emit_type_ref(f.ty);
				if (f.mode == ParamMode::Var || f.mode == ParamMode::Out || f.mode == ParamMode::Const)
					fprintf(out, "&");
				fprintf(out, " %s", f.cxx_name.c_str());
			}
			fprintf(out, ")");
			if (auto m = dynamic_cast<Method*>(call)) {
				if (m->virtual_kind == Method::VirtualKind::Override)
					fprintf(out, " override");
				if (m->virtual_kind == Method::VirtualKind::Abstract)
					fprintf(out, " = 0");
			}
			fprintf(out, ";\n");
		}
	}
	// Variant part: selector (if present) emits as a regular field; arms
	// collapse into a single anonymous union. See the strategy comment
	// above the body walk for the rationale.
	if (rec) {
		if (rec->has_selector) {
			fprintf(out, "\t");
			emit_type_ref(rec->selector_type);
			fprintf(out, " %s;\n", rec->selector_cxx_name.c_str());
		}
		if (!rec->arms.empty()) {
			fprintf(out, "\tunion {\n");
			for (auto& arm : rec->arms) {
				for (auto& f : arm.fields) {
					fprintf(out, "\t\t");
					emit_type_ref(f.ty);
					fprintf(out, " %s;\n", f.slot->cxx_name.c_str());
				}
			}
			fprintf(out, "\t};\n");
		}
	}
	fprintf(out, "}");
}

void Emitter::emit_type_definition(std::string cxx_name, Type* ty) {
	if (!out)
		return;
	if (auto e = dynamic_cast<EnumType*>(ty)) {
		// Pascal default is UNSCOPED enums: member identifiers leak into
		// the surrounding scope (where the type is declared) so a use like
		// `c := Red` resolves without qualification. C++ models this with
		// an unscoped `enum` (not `enum class`): members inject into the
		// enclosing namespace, exactly matching Pascal's semantics.
		fprintf(out, "\n");
		emit_enum_decl(e);
		fprintf(out, ";\n");
		return;
	}
	if (dynamic_cast<RecordType*>(ty) || dynamic_cast<ClassType*>(ty) || dynamic_cast<ObjectType*>(ty)) {
		fprintf(out, "\n");
		emit_aggregate_decl(cxx_name, ty);
		fprintf(out, ";\n");
		return;
	}
}

void Emitter::emit_type_alias(std::string cxx_name, std::string aliased_cxx_name) {
	if (!out)
		return;
	fprintf(out, "using %s = %s;\n", cxx_name.c_str(), aliased_cxx_name.c_str());
}

void Emitter::emit_procedure_close() {
	if (!out)
		return;
	fprintf(out, "}\n");
}

// Result-type of the whole expression drives the choice between bitwise and
// short-circuit for `and`/`or`; other ops have a unique mapping.
static const char* cxx_binary_operator(BinaryOperation* op) {
	bool booleans = op->ty == boolean_type();
	if (dynamic_cast<Add*>(op))
		return "+";
	if (dynamic_cast<Subtract*>(op))
		return "-";
	if (dynamic_cast<Multiply*>(op))
		return "*";
	if (dynamic_cast<Divide*>(op))
		return "/";
	if (dynamic_cast<Div*>(op))
		return "/";
	if (dynamic_cast<Mod*>(op))
		return "%";
	if (dynamic_cast<And*>(op))
		return booleans ? "&&" : "&";
	if (dynamic_cast<Or*>(op))
		return booleans ? "||" : "|";
	if (dynamic_cast<Xor*>(op))
		return "^";
	if (dynamic_cast<ShiftLeft*>(op))
		return "<<";
	if (dynamic_cast<ShiftRight*>(op))
		return ">>";
	if (dynamic_cast<Equal*>(op))
		return "==";
	if (dynamic_cast<NotEqual*>(op))
		return "!=";
	if (dynamic_cast<Less*>(op))
		return "<";
	if (dynamic_cast<Greater*>(op))
		return ">";
	if (dynamic_cast<LessOrEqual*>(op))
		return "<=";
	if (dynamic_cast<GreaterOrEqual*>(op))
		return ">=";
	return nullptr;
}

static const char* cxx_unary_operator(UnaryOperation* op) {
	if (dynamic_cast<Not*>(op))
		return "!";
	if (dynamic_cast<Negate*>(op))
		return "-";
	if (dynamic_cast<Positivize*>(op))
		return "+";
	if (dynamic_cast<AddrOf*>(op))
		return "&";
	if (dynamic_cast<Dereference*>(op))
		return "*";
	return nullptr;
}

void Emitter::emit_expression(Node* expr) {
	if (!out)
		return;
	if (auto c = dynamic_cast<Integer*>(expr)) {
		fprintf(out, "%llu", (unsigned long long)c->value);
		return;
	}
	if (auto s = dynamic_cast<String*>(expr)) {
		fputc('"', out);
		for (char ch : s->value) {
			if (ch == '"' || ch == '\\')
				fputc('\\', out);
			if ((unsigned char)ch < 0x20) {
				fprintf(out, "\\x%02x", (unsigned char)ch);
			} else {
				fputc(ch, out);
			}
		}
		fputc('"', out);
		return;
	}
	if (auto s = dynamic_cast<StorageSlot*>(expr)) {
		fprintf(out, "%s", s->cxx_name.c_str());
		return;
	}
	if (auto e = dynamic_cast<EnumMemberRef*>(expr)) {
		fprintf(out, "%s", e->cxx_name.c_str());
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
		// Use `->` when the container is (a) an explicit Dereference (collapse
		// `(*ptr).member` to `ptr->member`) or (b) a pointer-typed lvalue like
		// a method's implicit `this` slot.
		if (auto d = dynamic_cast<Dereference*>(m->a)) {
			emit_expression(d->a);
			fprintf(out, "->");
		} else if (m->a->ty && dynamic_cast<PointerType*>(m->a->ty)) {
			emit_expression(m->a);
			fprintf(out, "->");
		} else {
			emit_expression(m->a);
			fprintf(out, ".");
		}
		emit_expression(m->b);
		return;
	}
	if (auto ix = dynamic_cast<Index*>(expr)) {
		emit_expression(ix->a);
		fprintf(out, "[");
		emit_expression(ix->b);
		fprintf(out, "]");
		return;
	}
	if (auto pc = dynamic_cast<ProcCall*>(expr)) {
		if (pc->receiver) {
			// Same `->` conditions as MemberAccess: explicit Dereference of a
			// pointer, or a pointer-typed receiver (method `this` slot).
			if (auto d = dynamic_cast<Dereference*>(pc->receiver)) {
				emit_expression(d->a);
				fprintf(out, "->");
			} else if (pc->receiver->ty && dynamic_cast<PointerType*>(pc->receiver->ty)) {
				emit_expression(pc->receiver);
				fprintf(out, "->");
			} else {
				emit_expression(pc->receiver);
				fprintf(out, ".");
			}
		}
		emit_expression(pc->callee);
		fprintf(out, "(");
		for (size_t i = 0; i < pc->args.size(); i++) {
			if (i > 0)
				fprintf(out, ", ");
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
	if (!out)
		return;
	// Follow IncompleteType placeholders through to the real underlying type.
	while (auto inc = dynamic_cast<IncompleteType*>(ty)) {
		if (!inc->resolved)
			break;
		ty = inc->resolved;
	}
	if (auto it = dynamic_cast<IntrinsicType*>(ty)) {
		fprintf(out, "%.*s", (int)it->cxx_name.size(), it->cxx_name.data());
		return;
	}
	if (dynamic_cast<UnitType*>(ty)) {
		fprintf(out, "void");
		return;
	}
	if (auto r = dynamic_cast<RecordType*>(ty)) {
		if (r->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(out, "%s", r->cxx_name.c_str());
		return;
	}
	if (auto c = dynamic_cast<ClassType*>(ty)) {
		if (c->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(out, "%s", c->cxx_name.c_str());
		return;
	}
	if (auto o = dynamic_cast<ObjectType*>(ty)) {
		if (o->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(out, "%s", o->cxx_name.c_str());
		return;
	}
	if (auto e = dynamic_cast<EnumType*>(ty)) {
		// Anonymous inline enum (`var x: (A, B, C);`): emit the full
		// declaration inline so the member constants exist at this use
		// site. Two different anonymous enums share no type identity in
		// Pascal and we don't synthesise any here, so cross-use conflicts
		// would surface as g++ errors; named enums are the supported path.
		if (e->cxx_name.empty())
			emit_enum_decl(e);
		else
			fprintf(out, "%s", e->cxx_name.c_str());
		return;
	}
	if (auto p = dynamic_cast<PointerType*>(ty)) {
		emit_type_ref(p->item_type);
		fprintf(out, "*");
		return;
	}
	unhandled_type("emit_type_ref", ty);
}
