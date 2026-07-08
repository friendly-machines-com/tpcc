#include "emit.h"
#include "builtins.h"
#include "cst.h"
#include "frame.h"
#include "types.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <limits>
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

static bool fixed_array_length(Type* ty, uint64_t* out) {
	auto arr = dynamic_cast<FixedArrayType*>(ty);
	if (!arr)
		return false;
	*out = arr->range.length;
	return true;
}

static void emit_integer_literal(FILE* out, uint64_t value, bool negative) {
	if (negative) {
		if (value == (uint64_t{1} << 63))
			fprintf(out, "(-9223372036854775807ll - 1ll)");
		else
			fprintf(out, "-%llull", (unsigned long long)value);
	} else {
		fprintf(out, "%lluull", (unsigned long long)value);
	}
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
	if (!active)
		return;
	fprintf(active, "enum ");
	if (!e->cxx_name.empty())
		fprintf(active, "%s ", e->cxx_name.c_str());
	fprintf(active, "{ ");
	for (size_t i = 0; i < e->members.size(); i++) {
		if (i)
			fprintf(active, ", ");
		fprintf(active, "%s", e->members[i].cxx_name.c_str());
	}
	fprintf(active, " }");
}

// Apply the `p_` prefix to a Pascal value identifier.
std::string cxx_value_name(std::string pas_name) {
	return "p_" + pas_name;
}

// Apply the `t_` prefix to a Pascal type identifier.
std::string cxx_type_name(std::string pas_name) {
	return "t_" + pas_name;
}

Emitter::Emitter() : out_h(nullptr), out_cc(nullptr), active(nullptr), fresh_counter(0) {}

std::string Emitter::next_fresh_cxx_name(std::string prefix) {
	return prefix + "_" + std::to_string(++fresh_counter);
}

Emitter::~Emitter() {
	close();
}

void Emitter::open_for_program(std::string output_path) {
	out_cc = fopen(output_path.c_str(), "w");
	active = out_cc;
}

void Emitter::open_for_unit(std::string unit_name, std::string output_dir) {
	std::string base = output_dir.empty() ? (unit_name + ".") : (output_dir + "/" + unit_name + ".");
	out_h = fopen((base + "h").c_str(), "w");
	out_cc = fopen((base + "cc").c_str(), "w");
	// Units start parsing at the interface section.
	active = out_h;
}

void Emitter::set_section(Section s) {
	active = (s == Section::Header) ? out_h : out_cc;
}

void Emitter::close() {
	if (out_h) {
		fclose(out_h);
		out_h = nullptr;
	}
	if (out_cc) {
		fclose(out_cc);
		out_cc = nullptr;
	}
	active = nullptr;
}

void Emitter::emit_program_prologue(std::vector<std::string> used_unit_h_files) {
	if (!active)
		return;
	fprintf(active, "#include \"rtl.h\"\n");
	fprintf(active, "#include <functional>\n");
	for (auto& h : used_unit_h_files)
		fprintf(active, "#include \"%s\"\n", h.c_str());
	fprintf(active, "\n");
}

void Emitter::emit_unit_interface_prologue(std::vector<std::string> used_unit_h_files) {
	if (!active)
		return;
	fprintf(active, "#include \"rtl.h\"\n");
	fprintf(active, "#include <functional>\n");
	for (auto& h : used_unit_h_files)
		fprintf(active, "#include \"%s\"\n", h.c_str());
	fprintf(active, "\n");
}

void Emitter::emit_unit_implementation_prologue(std::string this_unit_h_file, std::vector<std::string> impl_used_unit_h_files) {
	if (!active)
		return;
	fprintf(active, "#include \"%s\"\n", this_unit_h_file.c_str());
	fprintf(active, "#include \"rtl.h\"\n");
	fprintf(active, "#include <functional>\n");
	for (auto& h : impl_used_unit_h_files)
		fprintf(active, "#include \"%s\"\n", h.c_str());
	fprintf(active, "\n");
}

void Emitter::emit_var_decl(std::string cxx_name, Type* ty) {
	if (!active)
		return;
	emit_type_ref(ty);
	fprintf(active, " %s;\n", cxx_name.c_str());
}

void Emitter::emit_main_prologue() {
	if (!active)
		return;
	fprintf(active, "\nint main() {\n");
}

void Emitter::emit_main_epilogue() {
	if (!active)
		return;
	fprintf(active, "\treturn 0;\n}\n");
}

// The cxx_name of the type that owns a Method, or empty if none.
static std::string owner_cxx_name(Type* owner) {
	if (auto r = dynamic_cast<RecordType*>(owner))
		return r->cxx_name;
	if (auto c = dynamic_cast<ClassType*>(owner))
		return c->cxx_name;
	if (auto i = dynamic_cast<InterfaceType*>(owner))
		return i->cxx_name;
	if (auto o = dynamic_cast<ObjectType*>(owner))
		return o->cxx_name;
	return "";
}

// Spelling of a Callable's C++ name token at any emit site. For destructors
// this is `~ClassName` so the token composes with `->` at the call site
// (obj->~Class()) and with `Owner::` for qualified destructor calls. Standard
// citations: [expr.prim.id.dtor] for the spelling, [expr.ref] for member
// access composition, [class.dtor]/15 for "A destructor can be called
// explicitly." For non-destructors, the value cxx name as-is.
static std::string callable_cxx_name(Callable* c) {
	if (c->ty->kind == DESTRUCTOR) {
		auto m = dynamic_cast<Method*>(c);
		return m && m->owner_class ? "~" + owner_cxx_name(m->owner_class) : "~";
	}
	return c->cxx_name;
}

void Emitter::emit_label(std::string cxx_label_name) {
	if (!active)
		return;
	fprintf(active, "%s:\n", cxx_label_name.c_str());
}

void Emitter::emit_goto(std::string cxx_label_name) {
	if (!active)
		return;
	fprintf(active, "\tgoto %s;\n", cxx_label_name.c_str());
}

void Emitter::emit_statement(Node* stmt) {
	if (!active)
		return;
	if (auto a = dynamic_cast<Assign*>(stmt)) {
		fprintf(active, "\t");
		emit_expression(a->a);
		fprintf(active, " = ");
		emit_expression(a->b);
		fprintf(active, ";\n");
		return;
	}
	if (auto pc = dynamic_cast<ProcCall*>(stmt)) {
		fprintf(active, "\t");
		emit_expression(pc);
		fprintf(active, ";\n");
		return;
	}
	if (auto r = dynamic_cast<Return*>(stmt)) {
		fprintf(active, "\treturn");
		if (r->a) {
			fprintf(active, " ");
			emit_expression(r->a);
		}
		fprintf(active, ";\n");
		return;
	}
	if (auto ic = dynamic_cast<InheritedCall*>(stmt)) {
		// `dropped` is set by the parser when the call would be redundant in
		// C++ (destructor-in-destructor auto-chains). Emit nothing.
		if (ic->dropped)
			return;
		auto m = dynamic_cast<Method*>(ic->resolved);
		if (!m || !m->owner_class)
			unhandled_node("inherited target is not a method", stmt);
		// Qualified-id `Parent::X(args)` -- C++ implicit-this injection makes
		// this a member call on `this`. See InheritedCall's docstring in cst.h.
		fprintf(active, "\t%s::%s(",
			owner_cxx_name(m->owner_class).c_str(),
			callable_cxx_name(ic->resolved).c_str());
		for (size_t i = 0; i < ic->args.size(); i++) {
			if (i > 0)
				fprintf(active, ", ");
			emit_expression(ic->args[i]);
		}
		fprintf(active, ");\n");
		return;
	}
	// Any other expression: evaluate and discard.
	fprintf(active, "\t");
	emit_expression(stmt);
	fprintf(active, ";\n");
}

void Emitter::emit_with_prologue(std::string alias_cxx_name, Node* target) {
	if (!active)
		return;
	// `auto&&` binds an lvalue target as a reference and lifetime-extends an
	// rvalue target (e.g. a function call returning a record by value), so the
	// with-body sees a single evaluation of the target expression regardless
	// of value category.
	fprintf(active, "\t{ auto&& %s = ", alias_cxx_name.c_str());
	emit_expression(target);
	fprintf(active, ";\n");
}

void Emitter::emit_with_epilogue() {
	if (!active)
		return;
	fprintf(active, "\t}\n");
}

void Emitter::emit_if_prologue(Node* condition) {
	if (!active)
		return;
	fprintf(active, "\tif (");
	emit_expression(condition);
	fprintf(active, ") {\n");
}

void Emitter::emit_if_else() {
	if (!active)
		return;
	fprintf(active, "\t} else {\n");
}

void Emitter::emit_if_epilogue() {
	if (!active)
		return;
	fprintf(active, "\t}\n");
}

void Emitter::emit_while_prologue(Node* condition) {
	if (!active)
		return;
	fprintf(active, "\twhile (");
	emit_expression(condition);
	fprintf(active, ") {\n");
}

void Emitter::emit_while_epilogue() {
	if (!active)
		return;
	fprintf(active, "\t}\n");
}

void Emitter::emit_repeat_prologue() {
	if (!active)
		return;
	fprintf(active, "\tdo {\n");
}

void Emitter::emit_repeat_epilogue(Node* condition) {
	if (!active)
		return;
	fprintf(active, "\t} while(!(");
	emit_expression(condition);
	fprintf(active, "));\n");
}

void Emitter::emit_routine_signature(RoutineType* ty, std::string cxx_text, Position pos, std::string owner_qualifier) {
	if (!active)
		return;
	if (pos == Position::DeclarationFormalsOnly) {
		fprintf(active, "(");
		for (size_t i = 0; i < ty->formals.size(); i++) {
			if (i > 0)
				fprintf(active, ", ");
			auto& f = ty->formals[i];
			if (f.mode == ParamMode::Const)
				fprintf(active, "const ");
			emit_type_ref(f.ty);
			if (f.mode == ParamMode::Var || f.mode == ParamMode::Out || f.mode == ParamMode::Const)
				fprintf(active, "&");
			fprintf(active, " %s", f.cxx_name.c_str());
		}
		fprintf(active, ")");
		return;
	}
	bool is_destructor = (ty->kind == DESTRUCTOR);
	if (!is_destructor) {
		emit_type_ref(ty->return_type);
		fprintf(active, " ");
	} else if (ty->return_type != &unit_type()) {
		unhandled_type("non-unit return type on destructor is not allowed", ty);
	}
	fprintf(active, "%s%s(", owner_qualifier.c_str(), cxx_text.c_str());
	for (size_t i = 0; i < ty->formals.size(); i++) {
		if (i > 0)
			fprintf(active, ", ");
		auto& f = ty->formals[i];
		if (f.mode == ParamMode::Const)
			fprintf(active, "const ");
		emit_type_ref(f.ty);
		if (f.mode == ParamMode::Var || f.mode == ParamMode::Out || f.mode == ParamMode::Const)
			fprintf(active, "&");
		fprintf(active, " %s", f.cxx_name.c_str());
	}
	fprintf(active, ")");
}

void Emitter::emit_callable_signature(Callable* c, Position pos, std::string owner_qualifier) {
	emit_routine_signature(c->ty, callable_cxx_name(c), pos, owner_qualifier);
}

void Emitter::emit_procedure_open(Callable* c) {
	if (!active)
		return;
	fprintf(active, "\n");
	std::string qualifier;
	if (auto m = dynamic_cast<Method*>(c)) {
		if (m->owner_class) {
			qualifier = owner_cxx_name(m->owner_class) + "::";
			if (c->ty->kind == CLASS_METHOD)
				qualifier += "m_meta::";
		}
	}
	emit_callable_signature(c, Position::Definition, qualifier);
	fprintf(active, " {\n");
	if (c->ty->return_type != &unit_type()) { // function
		fprintf(active, "\t");
		emit_type_ref(c->ty->return_type);
		fprintf(active, " p_result;\n");
	}
}

void Emitter::emit_procedure_close(Callable* target) {
	if (!active)
		return;
	auto ty = target->ty;
	if (ty->kind == CONSTRUCTOR) {
		fprintf(active, "\treturn this;\n");
	} else if (ty->return_type != &unit_type()) {
		fprintf(active, "\treturn p_result;\n");
	}
	fprintf(active, "}\n");
}

void Emitter::emit_callable_prototype(Callable* c, std::string owner_qualifier, std::string prefix, std::string suffix) {
	if (!active)
		return;
	fprintf(active, "%s", prefix.c_str());
	emit_callable_signature(c, Position::Declaration, owner_qualifier);
	fprintf(active, "%s;\n", suffix.c_str());
}

void Emitter::emit_aggregate_decl(std::string cxx_name, Type* ty, bool in_meta) {
	bool is_class = false;
	bool is_interface = false;
	if (!active)
		return;
	Frame* body = nullptr;
	const char* kw = "struct";
	RecordType* rec = nullptr;
	if (auto r = dynamic_cast<RecordType*>(ty)) {
		body = r->children;
		rec = r;
		kw = "struct";
	} else if (auto c = dynamic_cast<ClassType*>(ty)) {
		is_class = true;
		body = c->children;
		kw = "struct";
	} else if (auto i = dynamic_cast<InterfaceType*>(ty)) {
		is_interface = true;
		body = i->children;
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
	fprintf(active, "%s%s", attributes, kw);
	if (!cxx_name.empty())
		fprintf(active, " %s", cxx_name.c_str());

	// Base-class list emits LAYOUT names (the struct, not the storage pointer
	// form `t_foo*` that emit_type_ref would produce under the new model).
	// Spell the cxx_name directly. Skip the "public " prefix when there's no
	// super (avoids the pre-existing null-deref through emit_type_ref).
	if (auto c = dynamic_cast<ClassType*>(ty)) {
		bool first = true;
		if (c->super) {
			auto super_cxx_name = c->super->cxx_name;
			if (in_meta) {
				super_cxx_name = super_cxx_name + "::m_meta";
			}
				fprintf(active, " : public %s", super_cxx_name.c_str());
				first = false;
			}
			if (!in_meta) {
				for (auto interface_type : c->implemented_interfaces) {
					fprintf(active, first ? " : public %s" : ", public %s",
						interface_type->cxx_name.c_str());
					first = false;
				}
			} else {
			// not sure. FIXME: m_iobject ?
		}
	} else if (auto c = dynamic_cast<InterfaceType*>(ty)) {
			bool first = true;
			for (auto interface_type : c->super_interfaces) {
				fprintf(active, first ? " : public %s" : ", public %s",
					interface_type->cxx_name.c_str());
				first = false;
			}
		} else if (auto c = dynamic_cast<ObjectType*>(ty)) {
			if (c->super)
				fprintf(active, " : public %s", c->super->cxx_name.c_str());
		}

	fprintf(active, " {\n");
	if (is_class && in_meta) {
		if (auto c = dynamic_cast<ClassType*>(ty)) {
			std::string class_name = c->cxx_name;					// FIXME: terrible name.
			std::string parent_class_cxx_name = c->super ? c->super->cxx_name : ""; // FIXME: terrible name
			if (c->super && parent_class_cxx_name.empty()) {
				unhandled_type("parent class name unknown", c);
			}
			fprintf(active, "\tpublic: inline static m_meta* m_meta_instance() {\n");
			// This will basically NEVER be possible in Pascal.
			// Note: Alternative would be to emit "inline static struct m_meta { ... } meta;".
			fprintf(active, "\t\tstatic %s meta{};\n", cxx_name.c_str());
			fprintf(active, "\t\treturn &meta;\n");
			fprintf(active, "\t}\n");
			if (!body->lookup_value_local("classname")) {
				fprintf(active, "\tpublic: virtual inline ::pas::t_shortstring p_classname() {\n");
				fprintf(active, "\t\treturn ::pas::tpcc_shortstring_from_c(\"%s\");\n", class_name.c_str()); // FIXME: escape
				fprintf(active, "\t}\n");
			}
			if (!body->lookup_value_local("inheritsfrom")) {
				fprintf(active, "\tpublic: virtual inline bool p_inheritsfrom(::pas::t_tclass* s) {\n");
				if (parent_class_cxx_name.empty()) {
					fprintf(active, "\t\treturn s == this;\n");
				} else {
					fprintf(active, "\t\treturn s == this || %s::p_inheritsfrom(s);\n", parent_class_cxx_name.c_str()); // FIXME: escape
				}
				fprintf(active, "\t}\n");
			}
			if (!body->lookup_value_local("classparent")) {
				fprintf(active, "\tpublic: virtual inline ::pas::t_tclass* p_classparent() {\n");
				if (parent_class_cxx_name.empty()) {
					fprintf(active, "\t\treturn nullptr;\n");
				} else {
					fprintf(active, "\t\treturn %s::m_meta::m_meta_instance();\n", parent_class_cxx_name.c_str()); // FIXME: escape
				}
				fprintf(active, "\t}\n");
			}
			// TODO: maybe even add constructor wrappers here in the metaclass; they would do the (new X()).Create() and synth the result
			// fallthrough
		} else {
			unhandled_type("emit_aggregate_decl", ty);
		}
	} else if (is_class && !in_meta) {
		emit_aggregate_decl("m_meta", ty, true);
		fprintf(active, ";\n");
		// Generate wrapper proxies in the regular class.  Those all have to be generated each time since they are static.
		if (!body->lookup_value_local("classname")) {
			fprintf(active, "\tpublic: inline static ::pas::t_shortstring p_classname() {\n");
			fprintf(active, "\t\treturn m_meta::m_meta_instance()->p_classname();\n");
			fprintf(active, "\t}\n");
		}
		if (!body->lookup_value_local("inheritsfrom")) {
			fprintf(active, "\tpublic: inline static bool p_inheritsfrom(::pas::t_tclass* s) {\n");
			fprintf(active, "\t\treturn m_meta::m_meta_instance()->p_inheritsfrom(s);\n");
			fprintf(active, "\t}\n");
		}
		if (!body->lookup_value_local("classparent")) {
			fprintf(active, "\tpublic: inline static ::pas::t_tclass* p_classparent() {\n");
			fprintf(active, "\t\treturn m_meta::m_meta_instance()->p_classparent();\n");
			fprintf(active, "\t}\n");
		}
		// fallthrough
	}
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
	// FIXME: Frame's std::map iterates alphabetically; Pascal semantics require
	// source order for layout.
	for (auto& kv : body->values_local()) {
		Node* v = kv.second.value;
		if (auto slot = dynamic_cast<StorageSlot*>(v)) {
			if (is_interface) {
				unhandled_type("emit_aggregate_decl interfaces cannot have variables", ty);
				continue;
			}
			if (variant_slots.count(slot))
				continue;
			fprintf(active, "\t");
			emit_type_ref(slot->ty);
			fprintf(active, " %s;\n", slot->cxx_name.c_str());
		} else if (auto call = dynamic_cast<Callable*>(v)) {
			if (in_meta && call->ty->kind != CLASS_METHOD)
				continue;
			fprintf(active, "\t");
			if (auto m = dynamic_cast<Method*>(call)) {
				if (is_interface) {
					fprintf(active, "virtual ");
				} else if (call->ty->kind == CLASS_METHOD && !in_meta) {
					// autogenerate proxies in regular class
					fprintf(active, "inline static ");
				} else if (m->virtual_kind == Method::VirtualKind::Virtual || m->virtual_kind == Method::VirtualKind::Abstract || m->virtual_kind == Method::VirtualKind::Dynamic /*FIXME*/) {
					fprintf(active, "virtual ");
				}
			}
			emit_callable_signature(call, Position::Declaration, "");
			if (auto m = dynamic_cast<Method*>(call)) {
				if (is_interface) {
					fprintf(active, " = 0");
				} else if (call->ty->kind == CLASS_METHOD && !in_meta) {
					// Autogenerate proxies in regular class.  That's so the user can do: instance.foo() where foo is a class method.
					// C++ DOES allow calling instance.foo() this way even if instance's class doesnt have the static method but one of its superclasses does.
					fprintf(active, " {\n");
					fprintf(active, "\t%s m_meta::m_meta_instance()->%s(",
						call->ty->return_type == &unit_type() ? "" : "return",
						call->cxx_name.c_str()); // TODO: escape
					for (size_t i = 0; i < call->ty->formals.size(); i++) {
						auto& f = call->ty->formals[i];
						if (i > 0) {
							fprintf(active, ", ");
						}
						fprintf(active, " %s", f.cxx_name.c_str()); // TODO: escape
					}
					fprintf(active, "\t);\n");
					fprintf(active, "}\n");
				} else if (m->virtual_kind == Method::VirtualKind::Override) {
					fprintf(active, " override");
				} else if (m->virtual_kind == Method::VirtualKind::Abstract) {
					fprintf(active, " = 0");
				}
			}
			fprintf(active, ";\n");
		}
	}
	// Variant part: selector (if present) emits as a regular field; arms
	// collapse into a single anonymous union. See the strategy comment
	// above the body walk for the rationale.
	if (rec) {
		if (rec->has_selector) {
			fprintf(active, "\t");
			emit_type_ref(rec->selector_type);
			fprintf(active, " %s;\n", rec->selector_cxx_name.c_str());
		}
		if (!rec->arms.empty()) {
			fprintf(active, "\tunion {\n");
			for (auto& arm : rec->arms) {
				for (auto& f : arm.fields) {
					fprintf(active, "\t\t");
					emit_type_ref(f.ty);
					fprintf(active, " %s;\n", f.slot->cxx_name.c_str());
				}
			}
			fprintf(active, "\t};\n");
		}
	}
	fprintf(active, "}");
}

void Emitter::emit_type_definition(std::string cxx_name, Type* ty) {
	if (!active)
		return;
	if (auto e = dynamic_cast<EnumType*>(ty)) {
		// Pascal default is UNSCOPED enums: member identifiers leak into
		// the surrounding scope (where the type is declared) so a use like
		// `c := Red` resolves without qualification. C++ models this with
		// an unscoped `enum` (not `enum class`): members inject into the
		// enclosing namespace.
		fprintf(active, "\n");
		emit_enum_decl(e);
		fprintf(active, ";\n");
		return;
	}
	if (dynamic_cast<RecordType*>(ty) || dynamic_cast<ClassType*>(ty) || dynamic_cast<ObjectType*>(ty) || dynamic_cast<InterfaceType*>(ty)) {
		fprintf(active, "\n");
		emit_aggregate_decl(cxx_name, ty);
		fprintf(active, ";\n");
		return;
	}
}

void Emitter::emit_type_alias(std::string cxx_name, std::string aliased_cxx_name) {
	if (!active)
		return;
	fprintf(active, "using %s = %s;\n", cxx_name.c_str(), aliased_cxx_name.c_str());
}

void Emitter::emit_method_pointer_lambda(Node* obj_expr, Method* method) {
	auto rt = method->ty;
	// Reject ObjectType receivers. Class-type Self passes by pointer
	// (t_foo*), so by-value capture stores the pointer -- matches Pascal
	// TMethod.Data. ObjectType is value-typed; by-value capture would copy
	// the object, diverging from TMethod (which stores an address). Raise
	// rather than emit wrong code silently.
	if (obj_expr->ty && dynamic_cast<ObjectType*>(obj_expr->ty))
		unhandled_node("method-pointer capture of ObjectType receiver not supported (use a class type)", obj_expr);
	fprintf(active, "[");
	emit_expression(obj_expr);
	fprintf(active, "]");
	emit_routine_signature(rt, "", Position::DeclarationFormalsOnly, "");
	fprintf(active, " mutable { ");
	if (rt->return_type != &unit_type())
		fprintf(active, "return ");
	emit_expression(obj_expr);
	fprintf(active, "->%s(", callable_cxx_name(method).c_str());
	for (size_t i = 0; i < rt->formals.size(); i++) {
		if (i > 0)
			fprintf(active, ", ");
		fprintf(active, "%s", rt->formals[i].cxx_name.c_str());
	}
	fprintf(active, "); }");
}

static const char* cxx_unary_operator(UnaryOperation* op) {
	if (dynamic_cast<AddrOf*>(op))
		return "&";
	if (dynamic_cast<Dereference*>(op))
		return "*";
	return nullptr;
}

void Emitter::emit_expression(Node* expr) {
	if (!active)
		return;
	if (dynamic_cast<NilLiteral*>(expr)) {
		fprintf(active, "nullptr");
		return;
	}
	if (auto c = dynamic_cast<Integer*>(expr)) {
		emit_integer_literal(active, c->value, c->negative);
		return;
	}
	if (auto r = dynamic_cast<Real*>(expr)) {
		double inf = std::numeric_limits<double>::infinity();
		if (r->value != r->value)
			fprintf(active, "std::numeric_limits<double>::quiet_NaN()");
		else if (r->value == inf || r->value == -inf)
			fprintf(active, "%sstd::numeric_limits<double>::infinity()", r->value < 0 ? "-" : "");
		else
			fprintf(active, "%.17g", r->value);
		return;
	}
	if (auto s = dynamic_cast<String*>(expr)) {
		fputc('"', active);
		for (char ch : s->value) {
			if (ch == '"' || ch == '\\')
				fputc('\\', active);
			if ((unsigned char)ch < 0x20) {
				fprintf(active, "\\x%02x", (unsigned char)ch);
			} else {
				fputc(ch, active);
			}
		}
		fputc('"', active);
		return;
	}
	if (auto s = dynamic_cast<StorageSlot*>(expr)) {
		fprintf(active, "%s", s->cxx_name.c_str());
		return;
	}
	if (auto e = dynamic_cast<EnumMemberRef*>(expr)) {
		fprintf(active, "%s", e->cxx_name.c_str());
		return;
	}
	if (auto b = dynamic_cast<Builtin*>(expr)) {
		fprintf(active, "%.*s", (int)b->desc->cxx_name.size(), b->desc->cxx_name.data());
		return;
	}
	if (auto c = dynamic_cast<Callable*>(expr)) {
		fprintf(active, "%s", c->cxx_name.c_str());
		return;
	}
	if (auto m = dynamic_cast<MemberAccess*>(expr)) {
		// Use `->` when the container is (a) an explicit Dereference (collapse
		// `(*ptr).member` to `ptr->member`) or (b) a reference-typed lvalue
		// (Pascal `class`, `interface`, or `^T` -- all pointers in C++).
		if (auto d = dynamic_cast<Dereference*>(m->a)) {
			emit_expression(d->a);
			fprintf(active, "->");
		} else if (m->a->ty && m->a->ty->is_reference_type()) {
			emit_expression(m->a);
			fprintf(active, "->");
		} else {
			emit_expression(m->a);
			fprintf(active, ".");
		}
		emit_expression(m->b);
		return;
	}
	if (auto o = dynamic_cast<ShortCircuitOperation*>(expr)) {
		// TODO: support overloads, if any.
		fprintf(active, "((");
		emit_expression(o->a);
		switch (o->kind) {
		case AND:
			fprintf(active, ") && (");
			break;
		case OR:
			fprintf(active, ") || (");
			break;
		default:
			abort();
		}
		emit_expression(o->b);
		fprintf(active, "))");
		return;
	}
	if (auto ix = dynamic_cast<Index*>(expr)) {
		emit_expression(ix->a);
		fprintf(active, "[");
		emit_expression(ix->b);
		fprintf(active, "]");
		return;
	}
	if (auto pc = dynamic_cast<ProcCall*>(expr)) {
		if (pc->receiver) {
			auto receiver = pc->receiver;
			bool done = false;
			if (auto ty = dynamic_cast<RoutineType*>(pc->callee->ty)) {
				if (ty->kind == CONSTRUCTOR) {
					if (auto receiver_ty = dynamic_cast<ClassType*>(receiver->ty)) {
						fprintf(active, "(new %s", receiver_ty->cxx_name.c_str()); // FIXME: escape
						fprintf(active, ")->");
						done = true;
					} else {
						unhandled_type("constructor receiver", receiver->ty);
					}
				}
			}

			// Same `->` conditions as MemberAccess: explicit Dereference of a
			// pointer, or a reference-typed receiver (method `this` slot for a
			// class, or any `^T`-typed lvalue).
			if (done) {
			} else if (auto d = dynamic_cast<Dereference*>(receiver)) {
				emit_expression(d->a);
				fprintf(active, "->");
			} else if (receiver->ty && receiver->ty->is_reference_type()) {
				emit_expression(receiver);
				fprintf(active, "->");
			} else {
				emit_expression(receiver);
				fprintf(active, ".");
			}
		}
		if (auto c = dynamic_cast<Callable*>(pc->callee)) {
			fprintf(active, "%s(", callable_cxx_name(c).c_str());
		} else {
			emit_expression(pc->callee);
			fprintf(active, "(");
		}
		for (size_t i = 0; i < pc->args.size(); i++) {
			if (i > 0)
				fprintf(active, ", ");
			emit_expression(pc->args[i]);
		}
		fprintf(active, ")");
		return;
	}
	if (auto len = dynamic_cast<Length*>(expr)) {
		uint64_t array_len = 0;
		if (fixed_array_length(len->a ? len->a->ty : nullptr, &array_len)) {
			fprintf(active, "%llu", (unsigned long long)array_len);
		} else {
			emit_expression(len->a);
			fprintf(active, ".length");
		}
		return;
	}
	if (auto tb = dynamic_cast<TypeBound*>(expr)) {
		fprintf(active, tb->kind == TypeBoundKind::Low ? "pas::p_low<" : "pas::p_high<");
		emit_type_ref(tb->operand_type);
		fprintf(active, ">()");
		return;
	}
	if (auto ca = dynamic_cast<Cast*>(expr)) {
		fprintf(active, "static_cast<");
		emit_type_ref(ca->ty);
		fprintf(active, ">(");
		emit_expression(ca->a);
		fprintf(active, ")");
		return;
	}
	if (auto co = dynamic_cast<Coerce*>(expr)) {
		// FIXME: operator:= like ::cast ? I'm not sure what the difference between cast and coerce is in Pascal.
		// FIXME: emit assert(p_supports(co->a, co->ty))
		// FIXME: emit dynamic cast maybe ?
		fprintf(active, "dynamic_cast<");
		emit_type_ref(co->ty);
		fprintf(active, ">(");
		emit_expression(co->a);
		fprintf(active, ")");
		return;
	}
	if (auto co = dynamic_cast<CoerceCheck*>(expr)) {
		// FIXME: operator:= like ::cast ? I'm not sure what the difference between cast and coerce is in Pascal.
		// FIXME: emit assert(p_supports(co->a, co->ty))
		// FIXME: emit dynamic cast maybe ?
		fprintf(active, "(dynamic_cast<");
		emit_type_ref(co->ty);
		fprintf(active, ">(");
		emit_expression(co->a);
		fprintf(active, ") != nullptr)");
		return;
	}
	if (auto u = dynamic_cast<UnaryOperation*>(expr)) {
		// `@obj.method` for a `procedure of object`-typed LHS: render as a
		// lambda capturing obj by value and dispatching to the method. The
		// lambda converts implicitly to std::function<Ret(Args)> at the
		// assignment site. Plain `&expr` (function pointers, address-of a
		// standalone Callable) falls through to cxx_unary_operator.
		if (auto ao = dynamic_cast<AddrOf*>(u)) {
			if (auto ma = dynamic_cast<MemberAccess*>(ao->a)) {
				if (auto method = dynamic_cast<Method*>(ma->b)) {
					emit_method_pointer_lambda(ma->a, method);
					return;
				}
			}
		}
		if (const char* op = cxx_unary_operator(u)) {
			fprintf(active, "%s", op);
			emit_expression(u->a);
			return;
		}
	}
	if (auto ic = dynamic_cast<InheritedCall*>(expr)) {
		if (ic->dropped)
			unhandled_node("dropped inherited in expression context (destructor has no value)", expr);
		auto m = dynamic_cast<Method*>(ic->resolved);
		if (!m || !m->owner_class)
			unhandled_node("inherited target is not a method", expr);
		fprintf(active, "%s::%s(",
			owner_cxx_name(m->owner_class).c_str(),
			callable_cxx_name(ic->resolved).c_str());
		for (size_t i = 0; i < ic->args.size(); i++) {
			if (i > 0)
				fprintf(active, ", ");
			emit_expression(ic->args[i]);
		}
		fprintf(active, ")");
		return;
	}
	unhandled_node("emit_expression", expr);
}

void Emitter::emit_template_value_arg(Node* expr) {
	if (!active)
		return;
	if (auto i = dynamic_cast<Integer*>(expr)) {
		fprintf(active, "static_cast<");
		emit_type_ref(i->ty);
		fprintf(active, ">(");
		emit_integer_literal(active, i->value, i->negative);
		fprintf(active, ")");
		return;
	}
	if (auto e = dynamic_cast<EnumMemberRef*>(expr)) {
		fprintf(active, "%s", e->cxx_name.c_str());
		return;
	}
	unhandled_node("emit_template_value_arg", expr);
}

void Emitter::emit_type_ref(Type* ty) {
	if (!active)
		return;
	if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		// C++ doesn't support those, so punt for now.
		return emit_type_ref(s->base_type);
	}
	if (auto it = dynamic_cast<IntrinsicType*>(ty)) {
		fprintf(active, "%.*s", (int)it->cxx_name.size(), it->cxx_name.data());
		return;
	}
	if (dynamic_cast<UnitType*>(ty)) {
		fprintf(active, "void");
		return;
	}
	if (auto r = dynamic_cast<RecordType*>(ty)) {
		if (r->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(active, "%s", r->cxx_name.c_str());
		return;
	}
	if (auto c = dynamic_cast<ClassType*>(ty)) {
		if (c->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(active, "%s", c->cxx_name.c_str());
		fprintf(active, "*");
		return;
	}
	if (auto r = dynamic_cast<ClassRefType*>(ty)) {
		ty = r->target;
		if (auto c = dynamic_cast<ClassType*>(ty)) {
			if (c->cxx_name.empty())
				emit_aggregate_decl("", ty);
			else
				fprintf(active, "%s", c->cxx_name.c_str());
		} else {
			unhandled_type("emit_type_ref", ty);
		}
		fprintf(active, "::m_meta");
		fprintf(active, "*");
		return;
	}
	if (auto c = dynamic_cast<InterfaceType*>(ty)) {
		if (c->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(active, "%s", c->cxx_name.c_str());
		fprintf(active, "*");
		return;
	}
	if (auto o = dynamic_cast<ObjectType*>(ty)) {
		if (o->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(active, "%s", o->cxx_name.c_str());
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
			fprintf(active, "%s", e->cxx_name.c_str());
		return;
	}
	if (auto p = dynamic_cast<PointerType*>(ty)) {
		emit_type_ref(p->item_type);
		fprintf(active, "*");
		return;
	}
	if (auto rt = dynamic_cast<RoutineType*>(ty)) {
		// `procedure of object` (kind=METHOD): Pascal TMethod is a (Code, Data)
		// pair. std::function<Ret(Args)> type-erases that pair into a callable;
		// call site is uniform `m(args)` with function pointers. Wrapper emits
		// `std::function<` ... `>` around emit_routine_signature(name=""),
		// which produces `Ret (formals)`.
		//
		// Function pointer (kind=ROUTINE): `Ret (*)(Args)`. The `(*)` is the
		// pointer decoration, analogous to ClassType emitting as `t_foo*`.
		// Pass name="(*)" so emit_routine_signature emits `Ret (*)(formals)`.
		if (rt->kind == METHOD) {
			fprintf(active, "std::function<");
			emit_routine_signature(rt, "", Position::Declaration, "");
			fprintf(active, ">");
		} else {
			emit_routine_signature(rt, "(*)", Position::Declaration, "");
		}
		return;
	}
	if (auto s = dynamic_cast<FixedSetType*>(ty)) {
		emit_type_ref(set_type());
		return;
	}
	if (auto s = dynamic_cast<FixedArrayType*>(ty)) {
		emit_type_ref(fixedarray_type());
		fprintf(active, "<");
		emit_type_ref(s->item_type);
		fprintf(active, ", ");
		fprintf(active, "%llu", (unsigned long long)s->range.length);
		fprintf(active, ", ");
		emit_template_value_arg(s->range.lower_bound);
		fprintf(active, ">");
		return;
	}
	unhandled_type("emit_type_ref", ty);
}
