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

Emitter::Emitter() : out_h(nullptr), out_cc(nullptr), active(nullptr) {}

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
	fprintf(active, "#include <array>\n");
	fprintf(active, "#include <cstdlib>\n");
	fprintf(active, "#include <exception>\n");
	fprintf(active, "#include <functional>\n");
	for (auto& h : used_unit_h_files)
		fprintf(active, "#include \"%s\"\n", h.c_str());
	fprintf(active, "\n");
}

void Emitter::emit_unit_interface_prologue(std::vector<std::string> used_unit_h_files) {
	if (!active)
		return;
	fprintf(active, "#pragma once\n");
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

void Emitter::emit_unit_lifecycle_open(std::string cxx_name) {
	if (!active)
		return;
	fprintf(active, "\nvoid %s() {\n", cxx_name.c_str());
}

void Emitter::emit_unit_lifecycle_close() {
	if (!active)
		return;
	fprintf(active, "}\n");
}

void Emitter::emit_var_decl(std::string cxx_name, Type* ty) {
	if (!active)
		return;
	emit_type_ref(ty);
	fprintf(active, " %s;\n", cxx_name.c_str());
}

void Emitter::emit_const_decl(std::string cxx_name, Type* ty, Node* initializer) {
	if (!active)
		return;
	fprintf(active, "const ");
	emit_type_ref(ty);
	fprintf(active, " %s = ", cxx_name.c_str());
	emit_expression(initializer);
	fprintf(active, ";\n");
}

void Emitter::emit_main_prologue(
    const std::vector<std::pair<std::string, std::string>>&
        unit_lifecycle_hooks) {
	if (!active)
		return;
	fprintf(active, "\n");
	for (const auto& [initialize, finalize] :
	     unit_lifecycle_hooks) {
		fprintf(active, "void %s();\n", initialize.c_str());
		fprintf(active, "void %s();\n", finalize.c_str());
	}
	fprintf(active, "\nnamespace {\n");
	fprintf(active, "struct tpcc_unit_entry {\n");
	fprintf(active, "\tvoid (*initialize)();\n");
	fprintf(active, "\tvoid (*finalize)();\n");
	fprintf(active, "};\n\n");
	fprintf(active,
	        "constexpr std::array<tpcc_unit_entry, %zu> tpcc_units{{\n",
	        unit_lifecycle_hooks.size());
	for (const auto& [initialize, finalize] : unit_lifecycle_hooks)
		fprintf(active, "\t{%s, %s},\n",
		        initialize.c_str(), finalize.c_str());
	fprintf(active, "}};\n");
	fprintf(active, "std::size_t tpcc_initialized_unit_count = 0;\n");
	fprintf(active, "bool tpcc_finalization_started = false;\n\n");
	fprintf(active,
	        "void tpcc_finalize_initialized_units() noexcept {\n");
	fprintf(active, "\tif (tpcc_finalization_started)\n");
	fprintf(active, "\t\treturn;\n");
	fprintf(active, "\ttpcc_finalization_started = true;\n");
	fprintf(active, "\twhile (tpcc_initialized_unit_count != 0) {\n");
	fprintf(active, "\t\t--tpcc_initialized_unit_count;\n");
	fprintf(active,
	        "\t\tauto finalize = "
	        "tpcc_units[tpcc_initialized_unit_count].finalize;\n");
	fprintf(active, "\t\tif (finalize)\n");
	fprintf(active, "\t\t\tfinalize();\n");
	fprintf(active, "\t}\n");
	fprintf(active, "}\n");
	fprintf(active, "}\n\n");
	fprintf(active, "int main() {\n");
	fprintf(active,
	        "\tif (std::atexit(tpcc_finalize_initialized_units) != 0)\n");
	fprintf(active, "\t\tstd::terminate();\n");
	fprintf(active, "\ttry {\n");
	fprintf(active, "\t\tfor (const auto& unit : tpcc_units) {\n");
	fprintf(active, "\t\t\tif (unit.initialize)\n");
	fprintf(active, "\t\t\t\tunit.initialize();\n");
	fprintf(active, "\t\t\t++tpcc_initialized_unit_count;\n");
	fprintf(active, "\t\t}\n");
}

void Emitter::emit_main_epilogue() {
	if (!active)
		return;
	fprintf(active, "\t\treturn 0;\n");
	fprintf(active, "\t} catch (...) {\n");
	fprintf(active, "\t\ttpcc_finalize_initialized_units();\n");
	fprintf(active, "\t\tthrow;\n");
	fprintf(active, "\t}\n");
	fprintf(active, "}\n");
}

// The cxx_name of the type that owns a Method, or empty if none.
static std::string owner_cxx_name(Type* owner) {
	if (auto r = dynamic_cast<RecordType*>(owner))
		return r->cxx_name;
	if (auto r = dynamic_cast<PackedRecordType*>(owner))
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

static std::optional<std::pair<size_t, size_t>>
record_variant_path(Type* owner, StorageSlot* slot) {
	while (auto incomplete = dynamic_cast<IncompleteType*>(owner))
		owner = incomplete->resolved;
	auto record = dynamic_cast<RecordType*>(owner);
	if (!record || !slot)
		return std::nullopt;
	for (size_t arm_index = 0;
	     arm_index < record->arms.size(); ++arm_index) {
		const auto& arm = record->arms[arm_index];
		for (size_t field_index = 0;
		     field_index < arm.fields.size(); ++field_index)
			if (arm.fields[field_index].slot == slot)
				return std::pair{arm_index, field_index};
	}
	return std::nullopt;
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
		if (auto cast = dynamic_cast<Cast*>(a->a)) {
			// An assignable explicit ordinal cast is a same-sized view of an
			// existing Pascal place. Keep it out of ordinary C++ cast syntax:
			// a static_cast expression is not an lvalue, and reinterpret_cast
			// would create aliasing/lifetime hazards. The RTL helper bit-copies
			// the target value into a real source-carrier value, then assigns
			// that value through the typed storage view.
			fprintf(active, "\tpas::tpcc_store_writable_cast<");
			emit_type_ref(cast->ty);
			fprintf(active, ">(");
			emit_storage_ref(cast->a);
			fprintf(active, ", static_cast<");
			emit_type_ref(cast->ty);
			fprintf(active, ">(");
			emit_expression(a->b);
			fprintf(active, "));\n");
			return;
		}
		// Writable packed overlay, array-field element:
		//
		//   TPacked(source).bytes[index] := rhs
		//
		// The parser has already proved SOURCE is an assignable place. Bind it
		// once, copy into an aligned nominal carrier, update a copied array
		// field, put that field back, then synchronously copy the entire carrier
		// back to SOURCE. No reference to packed bytes is ever formed and no
		// temporary view can lose a delayed write.
		PropertyAccess* indexed_property = dynamic_cast<PropertyAccess*>(a->a);
		Node* indexed_receiver = indexed_property ? indexed_property->receiver : nullptr;
		Node* indexed_argument =
		    indexed_property && indexed_property->indexes.size() == 1
		        ? indexed_property->indexes.front()
		        : nullptr;
		if (auto ix = dynamic_cast<Index*>(a->a)) {
			indexed_receiver = ix->a;
			indexed_argument = ix->b;
		}
		if (indexed_receiver && indexed_argument) {
			if (auto m = dynamic_cast<MemberAccess*>(indexed_receiver)) {
				if (auto overlay = dynamic_cast<Cast*>(m->a)) {
					if (auto packed = dynamic_cast<PackedRecordType*>(overlay->ty)) {
						auto field = dynamic_cast<StorageSlot*>(m->b);
						if (!field)
							unhandled_node("packed overlay array target is not a field", m);
						if (packed->cxx_name.empty())
							unhandled_type("anonymous packed overlay", packed);
						fprintf(active, "\t[&]() {\n");
						fprintf(active, "\t\tauto&& tpcc_overlay_source = ");
						emit_expression(overlay->a);
						fprintf(active, ";\n");
						fprintf(active, "\t\tusing tpcc_overlay_source_type = std::remove_cvref_t<decltype(tpcc_overlay_source)>;\n");
						fprintf(active, "\t\tstatic_assert(std::is_trivially_copyable_v<tpcc_overlay_source_type>, \"packed overlay source must be trivially copyable\");\n");
						fprintf(active, "\t\tstatic_assert(sizeof(tpcc_overlay_source_type) == %s::m_storage_size, \"packed overlay size mismatch\");\n",
							packed->cxx_name.c_str());
						fprintf(active, "\t\t%s tpcc_overlay_value{};\n", packed->cxx_name.c_str());
						fprintf(active, "\t\tstd::memcpy(tpcc_overlay_value.m_data(), std::addressof(tpcc_overlay_source), sizeof(tpcc_overlay_source));\n");
						fprintf(active, "\t\tauto tpcc_overlay_field = tpcc_overlay_value.m_get_%s();\n", field->cxx_name.c_str());
						fprintf(active, "\t\tpas::p_index(tpcc_overlay_field, ");
						emit_expression(indexed_argument);
						fprintf(active, ") = ");
						emit_expression(a->b);
						fprintf(active, ";\n");
						fprintf(active, "\t\ttpcc_overlay_value.m_set_%s(tpcc_overlay_field);\n", field->cxx_name.c_str());
						fprintf(active, "\t\tstd::memcpy(std::addressof(tpcc_overlay_source), tpcc_overlay_value.m_data(), sizeof(tpcc_overlay_source));\n");
						fprintf(active, "\t}();\n");
						return;
					}
				}
			}
		}
		if (auto property = dynamic_cast<PropertyAccess*>(a->a)) {
			Node* accessor = property->property->write_accessor;
			if (!accessor)
				unhandled_node("assignment to read-only property", property);
			fprintf(active, "\t");
			if (auto field = dynamic_cast<StorageSlot*>(accessor)) {
				if (auto dereference = dynamic_cast<Dereference*>(property->receiver)) {
					emit_expression(dereference->a);
					fprintf(active, "->");
				} else if (property->receiver->ty && property->receiver->ty->is_reference_type()) {
					emit_expression(property->receiver);
					fprintf(active, "->");
				} else {
					emit_expression(property->receiver);
					fprintf(active, ".");
				}
				fprintf(active, "%s = ", field->cxx_name.c_str());
				emit_expression(a->b);
				fprintf(active, ";\n");
				return;
			}
			if (auto setter = dynamic_cast<Callable*>(accessor)) {
				if (auto dereference = dynamic_cast<Dereference*>(property->receiver)) {
					emit_expression(dereference->a);
					fprintf(active, "->");
				} else {
					emit_expression(property->receiver);
					fprintf(active, property->receiver->ty && property->receiver->ty->is_reference_type() ? "->" : ".");
				}
				fprintf(active, "%s(", callable_cxx_name(setter).c_str());
				for (size_t i = 0; i < property->indexes.size(); ++i) {
					if (i)
						fprintf(active, ", ");
					emit_expression(property->indexes[i]);
				}
				if (!property->indexes.empty())
					fprintf(active, ", ");
				emit_expression(a->b);
				fprintf(active, ");\n");
				return;
			}
			if (auto builtin = dynamic_cast<Builtin*>(accessor)) {
				fprintf(active, "%.*s(", (int)builtin->desc->cxx_name.size(), builtin->desc->cxx_name.data());
				emit_expression(property->receiver);
				for (Node* index : property->indexes) {
					fprintf(active, ", ");
					emit_expression(index);
				}
				fprintf(active, ") = ");
				emit_expression(a->b);
				fprintf(active, ";\n");
				return;
			}
			unhandled_node("unsupported property write accessor", accessor);
		}
		if (auto m = dynamic_cast<MemberAccess*>(a->a)) {
			if (dynamic_cast<PackedRecordType*>(m->a->ty)) {
				if (!dynamic_cast<StorageSlot*>(m->b))
					unhandled_node("packed-record assignment target is not a field", a->a);
				if (auto overlay = dynamic_cast<Cast*>(m->a)) {
					auto packed = static_cast<PackedRecordType*>(overlay->ty);
					if (packed->cxx_name.empty())
						unhandled_type("anonymous packed overlay", packed);
					auto field = static_cast<StorageSlot*>(m->b);
					fprintf(active, "\t[&]() {\n");
					fprintf(active, "\t\tauto&& tpcc_overlay_source = ");
					emit_expression(overlay->a);
					fprintf(active, ";\n");
					fprintf(active, "\t\tusing tpcc_overlay_source_type = std::remove_cvref_t<decltype(tpcc_overlay_source)>;\n");
					fprintf(active, "\t\tstatic_assert(std::is_trivially_copyable_v<tpcc_overlay_source_type>, \"packed overlay source must be trivially copyable\");\n");
					fprintf(active, "\t\tstatic_assert(sizeof(tpcc_overlay_source_type) == %s::m_storage_size, \"packed overlay size mismatch\");\n",
						packed->cxx_name.c_str());
					fprintf(active, "\t\t%s tpcc_overlay_value{};\n", packed->cxx_name.c_str());
					fprintf(active, "\t\tstd::memcpy(tpcc_overlay_value.m_data(), std::addressof(tpcc_overlay_source), sizeof(tpcc_overlay_source));\n");
					fprintf(active, "\t\ttpcc_overlay_value.m_set_%s(", field->cxx_name.c_str());
					emit_expression(a->b);
					fprintf(active, ");\n");
					fprintf(active, "\t\tstd::memcpy(std::addressof(tpcc_overlay_source), tpcc_overlay_value.m_data(), sizeof(tpcc_overlay_source));\n");
					fprintf(active, "\t}();\n");
					return;
				}
				fprintf(active, "\t");
				if (auto d = dynamic_cast<Dereference*>(m->a)) {
					emit_expression(d->a);
					fprintf(active, "->");
				} else {
					emit_expression(m->a);
					fprintf(active, ".");
				}
				fprintf(active, "m_set_%s(", static_cast<StorageSlot*>(m->b)->cxx_name.c_str());
				emit_expression(a->b);
				fprintf(active, ");\n");
				return;
			}
		}
		fprintf(active, "\t");
		emit_expression(a->a);
		fprintf(active, " = ");
		emit_expression(a->b);
		fprintf(active, ";\n");
		return;
	}
	if (auto write = dynamic_cast<WriteCall*>(stmt)) {
		fprintf(active, "\tpas::%s(",
		    write->newline ? "p_writeln" : "p_write");
		bool need_comma = false;
		if (write->file) {
			emit_writable_expression(write->file);
			need_comma = true;
		}
		for (const WriteCall::Item& item : write->items) {
			if (need_comma)
				fprintf(active, ", ");
			fprintf(active, "pas::tpcc_make_write_arg(");
			fprintf(active, "static_cast<");
			emit_type_ref(item.value->ty);
			fprintf(active, ">(");
			emit_expression(item.value);
			fprintf(active, ")");
			if (item.width) {
				fprintf(active, ", static_cast<pas::t_sizeint>(");
				emit_expression(item.width);
				fprintf(active, ")");
			}
			if (item.precision) {
				fprintf(active, ", static_cast<pas::t_sizeint>(");
				emit_expression(item.precision);
				fprintf(active, ")");
			}
			fprintf(active, ")");
			need_comma = true;
		}
		fprintf(active, ");\n");
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

void Emitter::emit_case_prologue(std::string selector_cxx_name, Node* selector) {
	if (!active)
		return;
	// Copy, rather than bind a reference: Pascal evaluates the selector to a
	// value once. A volatile or otherwise mutable lvalue must not be reread for
	// every arm comparison.
	fprintf(active, "\t{ auto %s = ", selector_cxx_name.c_str());
	emit_expression(selector);
	fprintf(active, ";\n");
}

void Emitter::emit_case_arm_prologue(Node* condition, bool first) {
	if (!active)
		return;
	fprintf(active, first ? "\tif (" : "\telse if (");
	emit_expression(condition);
	fprintf(active, ") {\n");
}

void Emitter::emit_case_arm_epilogue() {
	if (!active)
		return;
	fprintf(active, "\t}\n");
}

void Emitter::emit_case_else_prologue(bool has_previous_arm) {
	if (!active)
		return;
	fprintf(active, has_previous_arm ? "\telse {\n" : "\t{\n");
}

void Emitter::emit_case_epilogue() {
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

void Emitter::emit_for_prologue(Node* control, Node* initial, Node* final, bool descending) {
	if (!active)
		return;
	// Snapshot both bounds once. tpcc_for_done prevents the step after the
	// terminal iteration from overflowing at High(T)/Low(T); keeping the step
	// in the C++ for-increment expression also gives Pascal Continue its proper
	// "perform the loop step, then retest" behavior.
	fprintf(active, "\t{ ");
	emit_type_ref(control->ty);
	fprintf(active, " tpcc_for_initial = ");
	emit_expression(initial);
	fprintf(active, ";\n");
	fprintf(active, "\t");
	emit_type_ref(control->ty);
	fprintf(active, " tpcc_for_final = ");
	emit_expression(final);
	fprintf(active, ";\n");
	fprintf(active, "\tbool tpcc_for_done = false;\n");
	fprintf(active, "\tfor (");
	emit_expression(control);
	fprintf(active, " = tpcc_for_initial; !tpcc_for_done && pas::tpcc_for_%s_equal(",
		descending ? "greater" : "less");
	emit_expression(control);
	fprintf(active, ", tpcc_for_final); tpcc_for_done = pas::tpcc_for_equal(");
	emit_expression(control);
	fprintf(active, ", tpcc_for_final), ");
	emit_expression(control);
	fprintf(active, " = tpcc_for_done ? ");
	emit_expression(control);
	fprintf(active, " : pas::tpcc_for_%s(", descending ? "pred" : "succ");
	emit_expression(control);
	fprintf(active, ")) {\n");
}

void Emitter::emit_for_epilogue() {
	if (!active)
		return;
	fprintf(active, "\t}\n");
	fprintf(active, "\t}\n");
}

void Emitter::emit_loop_control(bool is_break) {
	if (!active)
		return;
	fprintf(active, is_break ? "\tbreak;\n" : "\tcontinue;\n");
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
			if (f.ty == unknown_type() &&
			    (f.mode == ParamMode::Var || f.mode == ParamMode::Out ||
			     f.mode == ParamMode::Const)) {
				fprintf(active, f.mode == ParamMode::Const
				    ? "pas::tpcc_const_storage_ref"
				    : "pas::tpcc_storage_ref");
			} else {
				if (f.mode == ParamMode::Const)
					fprintf(active, "const ");
				emit_type_ref(f.ty);
				if (f.mode == ParamMode::Var || f.mode == ParamMode::Out || f.mode == ParamMode::Const)
					fprintf(active, "&");
			}
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
		if (f.ty == unknown_type() &&
		    (f.mode == ParamMode::Var || f.mode == ParamMode::Out ||
		     f.mode == ParamMode::Const)) {
			fprintf(active, f.mode == ParamMode::Const
			    ? "pas::tpcc_const_storage_ref"
			    : "pas::tpcc_storage_ref");
		} else {
			if (f.mode == ParamMode::Const)
				fprintf(active, "const ");
			emit_type_ref(f.ty);
			if (f.mode == ParamMode::Var || f.mode == ParamMode::Out || f.mode == ParamMode::Const)
				fprintf(active, "&");
		}
		fprintf(active, " %s", f.cxx_name.c_str());
	}
	fprintf(active, ")");
}

void Emitter::emit_callable_signature(Callable* c, Position pos, std::string owner_qualifier) {
	emit_routine_signature(c->ty, callable_cxx_name(c), pos, owner_qualifier);
}

void Emitter::emit_procedure_open(Callable* c, bool nested_lambda) {
	if (!active)
		return;
	fprintf(active, "\n");
	if (nested_lambda) {
		fprintf(active, "\tauto %s = [&]", callable_cxx_name(c).c_str());
		emit_routine_signature(c->ty, "", Position::DeclarationFormalsOnly, "");
		if (c->ty->return_type != &unit_type()) {
			fprintf(active, " -> ");
			emit_type_ref(c->ty->return_type);
		}
		fprintf(active, " {\n");
		if (c->ty->return_type != &unit_type()) {
			fprintf(active, "\t");
			emit_type_ref(c->ty->return_type);
			fprintf(active, " p_result;\n");
		}
		return;
	}
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

void Emitter::emit_procedure_close(Callable* target, bool nested_lambda) {
	if (!active)
		return;
	auto ty = target->ty;
	if (ty->kind == CONSTRUCTOR) {
		fprintf(active, "\treturn this;\n");
	} else if (ty->return_type != &unit_type()) {
		fprintf(active, "\treturn p_result;\n");
	}
	fprintf(active, nested_lambda ? "\t};\n" : "}\n");
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
	fprintf(active, "%s", kw);
	if (!cxx_name.empty())
		fprintf(active, " %s", cxx_name.c_str());

	// Base-class lists emit layout names (the struct, not the storage pointer
	// form `t_foo*` that emit_type_ref would produce under the new model).
	// The first base needs the C++ `:` introducer; later bases use commas.
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
	auto classref_api_cxx_for = [&](ClassType* c) -> std::string {
		ClassType* target = c;
		while (target->super)
			target = target->super;
		if (target->cxx_name.empty())
			unhandled_type("metaclass API target name unknown", target);
		return target->cxx_name + "::m_meta*";
	};
	if (is_class && in_meta) {
		if (auto c = dynamic_cast<ClassType*>(ty)) {
			std::string class_name = c->cxx_name;					// FIXME: terrible name.
			std::string parent_class_cxx_name = c->super ? c->super->cxx_name : ""; // FIXME: terrible name
			if (c->super && parent_class_cxx_name.empty()) {
				unhandled_type("parent class name unknown", c);
			}
			std::string classref_api_cxx = classref_api_cxx_for(c);
			fprintf(active, "\tpublic: inline static m_meta* m_meta_instance() {\n");
			// This will basically NEVER be possible in Pascal.
			// Note: Alternative would be to emit "inline static struct m_meta { ... } meta;".
			fprintf(active, "\t\tstatic %s meta{};\n", cxx_name.c_str());
			fprintf(active, "\t\treturn &meta;\n");
			fprintf(active, "\t}\n");
			if (!body->lookup_value_local("classname")) {
				fprintf(active, "\tpublic: virtual inline ::pas::t_shortstring p_classname() {\n");
				fprintf(active,
					"\t\treturn ::pas::tpcc_shortstring_from_c(\"%s\", strlen(\"%s\"));\n",
					class_name.c_str(), class_name.c_str()); // FIXME: escape
				fprintf(active, "\t}\n");
			}
			if (!body->lookup_value_local("inheritsfrom")) {
				fprintf(active, "\tpublic: virtual inline ::pas::t_boolean p_inheritsfrom(%s s) {\n", classref_api_cxx.c_str());
				if (parent_class_cxx_name.empty()) {
					fprintf(active, "\t\treturn ::pas::tpcc_bool_to_boolean(s == this);\n");
				} else {
					fprintf(active, "\t\treturn ::pas::tpcc_bool_to_boolean(s == this || %s::p_inheritsfrom(s));\n", parent_class_cxx_name.c_str()); // FIXME: escape
				}
				fprintf(active, "\t}\n");
			}
			if (!body->lookup_value_local("classparent")) {
				fprintf(active, "\tpublic: virtual inline %s p_classparent() {\n", classref_api_cxx.c_str());
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
		auto c = static_cast<ClassType*>(ty);
		std::string classref_api_cxx = classref_api_cxx_for(c);
		emit_aggregate_decl("m_meta", ty, true);
		fprintf(active, ";\n");
		// Generate wrapper proxies in the regular class.  Those all have to be generated each time since they are static.
		if (!body->lookup_value_local("classname")) {
			fprintf(active, "\tpublic: inline static ::pas::t_shortstring p_classname() {\n");
			fprintf(active, "\t\treturn m_meta::m_meta_instance()->p_classname();\n");
			fprintf(active, "\t}\n");
		}
		if (!body->lookup_value_local("inheritsfrom")) {
			fprintf(active, "\tpublic: inline static ::pas::t_boolean p_inheritsfrom(%s s) {\n", classref_api_cxx.c_str());
			fprintf(active, "\t\treturn m_meta::m_meta_instance()->p_inheritsfrom(s);\n");
			fprintf(active, "\t}\n");
		}
		if (!body->lookup_value_local("classparent")) {
			fprintf(active, "\tpublic: inline static %s p_classparent() {\n", classref_api_cxx.c_str());
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
	//   ordinary struct members. Each Pascal arm becomes a source-ordered C++
	//   struct, and one union contains those arm structs. Fields within an arm
	//   are therefore sequential; different arms overlap at the union offset.
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
	if (rec) {
		for (const auto& field : rec->fields) {
			fprintf(active, "\t");
			emit_type_ref(field.ty);
			fprintf(active, " %s;\n", field.slot->cxx_name.c_str());
		}
	}
	for (auto& kv : body->values_local()) {
		Node* v = kv.second.value;
		if (auto slot = dynamic_cast<StorageSlot*>(v)) {
			if (is_interface) {
				unhandled_type("emit_aggregate_decl interfaces cannot have variables", ty);
				continue;
			}
			if (variant_slots.count(slot))
				continue;
			if (rec)
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
			for (size_t arm_index = 0;
			     arm_index < rec->arms.size(); ++arm_index) {
				fprintf(active,
				    "\tstruct m_variant_arm_%zu_type {\n",
				    arm_index);
				for (auto& f : rec->arms[arm_index].fields) {
					fprintf(active, "\t\t");
					emit_type_ref(f.ty);
					fprintf(active, " %s;\n", f.slot->cxx_name.c_str());
				}
				fprintf(active, "\t};\n");
			}
			fprintf(active, "\tunion m_variant_type {\n");
			for (size_t arm_index = 0;
			     arm_index < rec->arms.size(); ++arm_index) {
				fprintf(active,
				    "\t\tm_variant_arm_%zu_type m_arm_%zu;\n",
				    arm_index, arm_index);
			}
			fprintf(active, "\t} m_variant;\n");
		}
	}
	fprintf(active, "}");
}

void Emitter::emit_packed_record_decl(std::string cxx_name, PackedRecordType* p) {
	if (!active)
		return;
	if (cxx_name.empty())
		unhandled_type("anonymous packed records are not implemented", p);

	fprintf(active, "struct %s {\n", cxx_name.c_str());
	for (size_t i = 0; i < p->fields.size(); ++i) {
		auto& field = p->fields[i];
		fprintf(active, "\tusing m_field_%zu_type = ", i);
		emit_type_ref(field.ty);
		fprintf(active, ";\n");
		// Use enumerators rather than `inline static constexpr` data members.
		// Pascal permits a packed-record type declaration inside a routine
		// (cutils.pas does this for TWordRec/TLongWordRec), which makes the
		// emitted C++ struct a local class. C++20 [class.local] forbids static
		// data members in local classes, including inline constexpr ones.
		// An enumerator is not a data member, remains an integral constant
		// expression, and is still addressable syntactically as
		// `PackedType::m_field_N_offset`.
		fprintf(active, "\tenum : std::size_t { m_field_%zu_offset = ", i);
		if (i == 0)
			fprintf(active, "0");
		else
			fprintf(active, "m_field_%zu_offset + sizeof(m_field_%zu_type)", i - 1, i - 1);
		fprintf(active, " };\n");
	}
	// Same local-class restriction as the field offsets above.
	fprintf(active, "\tenum : std::size_t { m_storage_size = ");
	if (p->fields.empty())
		fprintf(active, "1");
	else
		fprintf(active, "m_field_%zu_offset + sizeof(m_field_%zu_type)", p->fields.size() - 1, p->fields.size() - 1);
	fprintf(active, " };\n");
	fprintf(active, "\nprivate:\n");
	fprintf(active, "\tstd::array<std::byte, m_storage_size> m_storage{};\n");
	fprintf(active, "\npublic:\n");
	fprintf(active, "\tstd::byte* m_data() noexcept { return m_storage.data(); }\n");
	fprintf(active, "\tconst std::byte* m_data() const noexcept { return m_storage.data(); }\n");
	for (size_t i = 0; i < p->fields.size(); ++i) {
		auto& field = p->fields[i];
		fprintf(active, "\tm_field_%zu_type m_get_%s() const noexcept {\n", i, field.slot->cxx_name.c_str());
		fprintf(active, "\t\tstatic_assert(std::is_trivially_copyable_v<m_field_%zu_type>, \"packed field must be trivially copyable\");\n", i);
		fprintf(active, "\t\tstatic_assert(m_field_%zu_offset + sizeof(m_field_%zu_type) <= m_storage_size, \"packed field exceeds carrier storage\");\n", i, i);
		fprintf(active, "\t\tm_field_%zu_type value{};\n", i);
		fprintf(active, "\t\tstd::memcpy(&value, m_storage.data() + m_field_%zu_offset, sizeof value);\n", i);
		fprintf(active, "\t\treturn value;\n");
		fprintf(active, "\t}\n");
		fprintf(active, "\tvoid m_set_%s(const m_field_%zu_type& value) noexcept {\n", field.slot->cxx_name.c_str(), i);
		fprintf(active, "\t\tstatic_assert(std::is_trivially_copyable_v<m_field_%zu_type>, \"packed field must be trivially copyable\");\n", i);
		fprintf(active, "\t\tstatic_assert(m_field_%zu_offset + sizeof(m_field_%zu_type) <= m_storage_size, \"packed field exceeds carrier storage\");\n", i, i);
		fprintf(active, "\t\tstd::memcpy(m_storage.data() + m_field_%zu_offset, &value, sizeof value);\n", i);
		fprintf(active, "\t}\n");
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
	if (auto p = dynamic_cast<PackedRecordType*>(ty)) {
		fprintf(active, "\n");
		emit_packed_record_decl(cxx_name, p);
		fprintf(active, ";\n");
		fprintf(active, "static_assert(sizeof(%s) == %s::m_storage_size, \"packed-record carrier size mismatch\");\n",
			cxx_name.c_str(), cxx_name.c_str());
		fprintf(active, "static_assert(alignof(%s) == 1, \"packed-record carrier alignment mismatch\");\n",
			cxx_name.c_str());
		fprintf(active, "static_assert(std::is_standard_layout_v<%s>, \"packed-record carrier must have standard layout\");\n",
			cxx_name.c_str());
		fprintf(active, "static_assert(std::is_trivially_copyable_v<%s>, \"packed-record carrier must be trivially copyable\");\n",
			cxx_name.c_str());
		return;
	}
	if (dynamic_cast<RecordType*>(ty) || dynamic_cast<ClassType*>(ty) || dynamic_cast<ObjectType*>(ty) || dynamic_cast<InterfaceType*>(ty)) {
		fprintf(active, "\n");
		emit_aggregate_decl(cxx_name, ty);
		fprintf(active, ";\n");
		if (auto record = dynamic_cast<RecordType*>(ty)) {
			auto layout = record_layout(record);
			if (!layout)
				unhandled_type(
				    "ordinary record layout is not known", record);
			fprintf(active,
			    "static_assert(std::is_standard_layout_v<%s>, \"ordinary record must have standard layout\");\n",
			    cxx_name.c_str());
			auto find_layout =
			    [&](StorageSlot* slot) -> const AggregateFieldLayout* {
				for (const auto& field : layout->fields)
					if (field.slot == slot)
						return &field;
				return nullptr;
			};
			auto emit_field_assertions =
			    [&](StorageSlot* slot, const char* member_expression,
			        const char* type_expression) {
				const auto* field = find_layout(slot);
				if (!field)
					unhandled_node(
					    "ordinary record field has no layout", slot);
				fprintf(active,
				    "static_assert(%s == %llu, \"ordinary-record field offset mismatch\");\n",
				    member_expression,
				    (unsigned long long)field->offset);
				fprintf(active,
				    "static_assert(sizeof(%s) == %llu, \"ordinary-record field size mismatch\");\n",
				    type_expression,
				    (unsigned long long)field->size);
			};
			for (const auto& field : record->fields) {
				std::string offset =
				    "offsetof(" + cxx_name + ", " +
				    field.slot->cxx_name + ")";
				std::string type =
				    "decltype(" + cxx_name + "::" +
				    field.slot->cxx_name + ")";
				emit_field_assertions(
				    field.slot, offset.c_str(), type.c_str());
			}
			if (record->has_selector) {
				std::string offset =
				    "offsetof(" + cxx_name + ", " +
				    record->selector_cxx_name + ")";
				std::string type =
				    "decltype(" + cxx_name + "::" +
				    record->selector_cxx_name + ")";
				emit_field_assertions(
				    record->selector_slot,
				    offset.c_str(), type.c_str());
			}
			for (size_t arm_index = 0;
			     arm_index < record->arms.size(); ++arm_index) {
				for (const auto& field :
				     record->arms[arm_index].fields) {
					std::string offset =
					    "offsetof(" + cxx_name +
					    ", m_variant) + offsetof(" +
					    cxx_name + "::m_variant_arm_" +
					    std::to_string(arm_index) +
					    "_type, " +
					    field.slot->cxx_name + ")";
					std::string type =
					    "decltype(" + cxx_name +
					    "::m_variant_arm_" +
					    std::to_string(arm_index) +
					    "_type::" +
					    field.slot->cxx_name + ")";
					emit_field_assertions(
					    field.slot, offset.c_str(),
					    type.c_str());
				}
			}
			fprintf(active,
			    "static_assert(sizeof(%s) == %llu, \"ordinary-record total size mismatch\");\n",
			    cxx_name.c_str(),
			    (unsigned long long)layout->type.size);
			fprintf(active,
			    "static_assert(alignof(%s) == %llu, \"ordinary-record alignment mismatch\");\n",
			    cxx_name.c_str(),
			    (unsigned long long)layout->type.alignment);
		}
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

void Emitter::emit_writable_expression(Node* expr) {
	auto property = dynamic_cast<PropertyAccess*>(expr);
	if (!property) {
		emit_expression(expr);
		return;
	}
	auto builtin = dynamic_cast<Builtin*>(property->property->write_accessor);
	if (!builtin) {
		// Field-backed properties already emit their underlying place through
		// the normal expression path. Method-backed properties are not
		// referenceable and therefore cannot reach this function.
		emit_expression(expr);
		return;
	}
	fprintf(active, "%.*s(", (int)builtin->desc->cxx_name.size(),
		builtin->desc->cxx_name.data());
	emit_expression(property->receiver);
	for (Node* index : property->indexes) {
		fprintf(active, ", ");
		emit_expression(index);
	}
	fprintf(active, ")");
}

void Emitter::emit_storage_ref(Node* expr) {
	if (auto property = dynamic_cast<PropertyAccess*>(expr)) {
		if (dynamic_cast<Builtin*>(property->property->write_accessor)) {
			fprintf(active, "pas::tpcc_make_storage_ref(");
			emit_expression(property->receiver);
			for (Node* index : property->indexes) {
				fprintf(active, ", ");
				emit_expression(index);
			}
			fprintf(active, ")");
			return;
		}
	}
	fprintf(active, "pas::tpcc_make_storage_ref(");
	emit_writable_expression(expr);
	fprintf(active, ")");
}

void Emitter::emit_const_storage_ref(Node* expr) {
	if (auto property = dynamic_cast<PropertyAccess*>(expr)) {
		if (dynamic_cast<Builtin*>(property->property->read_accessor)) {
			fprintf(active, "pas::tpcc_make_const_storage_ref(");
			emit_expression(property->receiver);
			for (Node* index : property->indexes) {
				fprintf(active, ", ");
				emit_expression(index);
			}
			fprintf(active, ")");
			return;
		}
	}
	fprintf(active, "pas::tpcc_make_const_storage_ref(");
	emit_expression(expr);
	fprintf(active, ")");
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
		long double inf = std::numeric_limits<long double>::infinity();
		if (r->value != r->value) {
			fprintf(active, "std::numeric_limits<");
			emit_type_ref(r->ty);
			fprintf(active, ">::quiet_NaN()");
		} else if (r->value == inf || r->value == -inf) {
			fprintf(active, "%sstd::numeric_limits<", r->value < 0 ? "-" : "");
			emit_type_ref(r->ty);
			fprintf(active, ">::infinity()");
		} else {
			fprintf(active, "static_cast<");
			emit_type_ref(r->ty);
			fprintf(active, ">(");
			fprintf(active, "%.*Lg", std::numeric_limits<long double>::max_digits10, r->value);
			fprintf(active, ")");
		}
		return;
	}
	if (auto s = dynamic_cast<String*>(expr)) {
		if (s->ty == char_type()) {
			if (s->value.size() != 1)
				unhandled_node("Char literal does not contain exactly one byte", s);
			fprintf(active, "static_cast<pas::t_char>(static_cast<uint8_t>(%u))",
				static_cast<unsigned>(static_cast<unsigned char>(s->value[0])));
			return;
		}
		fprintf(active, "pas::tpcc_shortstring_from_c(");
		fputc('"', active);
		for (unsigned char ch : s->value)
			fprintf(active, "\\%03o", static_cast<unsigned>(ch));
		fputc('"', active);
		fprintf(active, ", %zu)", s->value.size());
		return;
	}
	if (auto a = dynamic_cast<FixedArrayLiteral*>(expr)) {
		fprintf(active, "{{");
		for (size_t i = 0; i < a->elements.size(); i++) {
			if (i > 0)
				fprintf(active, ", ");
			emit_expression(a->elements[i]);
		}
		fprintf(active, "}}");
		return;
	}
	if (auto set = dynamic_cast<SetLiteral*>(expr)) {
		auto set_type = dynamic_cast<FixedSetType*>(set->ty);
		if (!set_type || set_type->item_type == unknown_type())
			unhandled_node("set literal has no contextual item type", set);
		fprintf(active, "pas::tpcc_make_set<");
		emit_type_ref(set_type->item_type);
		fprintf(active, ">({");
		for (size_t i = 0; i < set->items.size(); ++i) {
			if (i)
				fprintf(active, ", ");
			const SetLiteral::Item& item = set->items[i];
			if (item.upper) {
				fprintf(active, "pas::tpcc_set_range(");
				emit_expression(item.lower);
				fprintf(active, ", ");
				emit_expression(item.upper);
				fprintf(active, ")");
			} else {
				fprintf(active, "pas::tpcc_set_single(");
				emit_expression(item.lower);
				fprintf(active, ")");
			}
		}
		fprintf(active, "})");
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
	if (auto property = dynamic_cast<PropertyAccess*>(expr)) {
		Node* accessor = property->property->read_accessor;
		if (!accessor)
			unhandled_node("read from write-only property", property);
		if (auto field = dynamic_cast<StorageSlot*>(accessor)) {
			if (auto dereference = dynamic_cast<Dereference*>(property->receiver)) {
				emit_expression(dereference->a);
				fprintf(active, "->");
			} else {
				emit_expression(property->receiver);
				fprintf(active, property->receiver->ty && property->receiver->ty->is_reference_type() ? "->" : ".");
			}
			fprintf(active, "%s", field->cxx_name.c_str());
			return;
		}
		if (auto getter = dynamic_cast<Callable*>(accessor)) {
			if (auto dereference = dynamic_cast<Dereference*>(property->receiver)) {
				emit_expression(dereference->a);
				fprintf(active, "->");
			} else {
				emit_expression(property->receiver);
				fprintf(active, property->receiver->ty && property->receiver->ty->is_reference_type() ? "->" : ".");
			}
			fprintf(active, "%s(", callable_cxx_name(getter).c_str());
			for (size_t i = 0; i < property->indexes.size(); ++i) {
				if (i)
					fprintf(active, ", ");
				emit_expression(property->indexes[i]);
			}
			fprintf(active, ")");
			return;
		}
		if (auto builtin = dynamic_cast<Builtin*>(accessor)) {
			fprintf(active, "%.*s(", (int)builtin->desc->cxx_name.size(), builtin->desc->cxx_name.data());
			emit_expression(property->receiver);
			for (Node* index : property->indexes) {
				fprintf(active, ", ");
				emit_expression(index);
			}
			fprintf(active, ")");
			return;
		}
		unhandled_node("unsupported property read accessor", accessor);
	}
	if (auto m = dynamic_cast<MemberAccess*>(expr)) {
		if (auto packed = dynamic_cast<PackedRecordType*>(m->a->ty)) {
			(void)packed;
			auto field = dynamic_cast<StorageSlot*>(m->b);
			if (!field)
				unhandled_node("packed-record member is not a field", expr);
			if (auto d = dynamic_cast<Dereference*>(m->a)) {
				emit_expression(d->a);
				fprintf(active, "->");
			} else {
				emit_expression(m->a);
				fprintf(active, ".");
			}
			fprintf(active, "m_get_%s()", field->cxx_name.c_str());
			return;
		}
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
		if (auto slot = dynamic_cast<StorageSlot*>(m->b)) {
			if (auto path =
			        record_variant_path(m->a->ty, slot)) {
				fprintf(active,
				    "m_variant.m_arm_%zu.",
				    path->first);
			}
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
		fprintf(active, "pas::p_index(");
		emit_expression(ix->a);
		fprintf(active, ", ");
		emit_expression(ix->b);
		fprintf(active, ")");
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
			Node* arg = pc->args[i];
			// Callable has its semantic RoutineType in Callable::ty (the
			// historical class currently shadows Node::ty), so recover it
			// through the concrete callee rather than reading Node::ty.
			RoutineType* call_ty = nullptr;
			if (auto callable = dynamic_cast<Callable*>(pc->callee)) {
				call_ty = callable->ty;
			} else {
				call_ty = dynamic_cast<RoutineType*>(pc->callee->ty);
			}
			if (call_ty && i < call_ty->formals.size() &&
			    call_ty->formals[i].mode == ParamMode::Value &&
			    dynamic_cast<Integer*>(arg)) {
				Type* formal_ty = call_ty->formals[i].ty;
				// The Pascal overload has already been selected. Spell its
				// value-parameter type at the C++ call boundary so a raw
				// literal such as `3ull` cannot make C++ independently choose
				// among every p_equal/p_divide overload. Non-literal implicit
				// conversions are already explicit Cast nodes; variables
				// already have their declared C++ type. Do not value-cast
				// var/out/const arguments: those must remain references.
				fprintf(active, "static_cast<");
				emit_type_ref(formal_ty);
				fprintf(active, ">(");
				emit_expression(arg);
				fprintf(active, ")");
			} else if (call_ty && i < call_ty->formals.size() &&
				   call_ty->formals[i].mode == ParamMode::Const &&
				   call_ty->formals[i].ty == unknown_type()) {
				emit_const_storage_ref(arg);
			} else if (call_ty && i < call_ty->formals.size() &&
				   (call_ty->formals[i].mode == ParamMode::Var ||
				    call_ty->formals[i].mode == ParamMode::Out) &&
				   call_ty->formals[i].ty == unknown_type()) {
				emit_storage_ref(arg);
			} else if (call_ty && i < call_ty->formals.size() &&
				   (call_ty->formals[i].mode == ParamMode::Var ||
				    call_ty->formals[i].mode == ParamMode::Out)) {
				emit_writable_expression(arg);
			} else {
				emit_expression(arg);
			}
		}
		fprintf(active, ")");
		return;
	}
	if (auto tb = dynamic_cast<TypeBound*>(expr)) {
		fprintf(active, tb->kind == TypeBoundKind::Low ? "pas::p_low<" : "pas::p_high<");
		emit_type_ref(tb->operand_type);
		fprintf(active, ">()");
		return;
	}
	if (auto size = dynamic_cast<SizeOf*>(expr)) {
		fprintf(active, "static_cast<pas::t_sizeint>(sizeof(");
		emit_type_ref(size->operand_type);
		fprintf(active, "))");
		return;
	}
	if (auto ca = dynamic_cast<Cast*>(expr)) {
		if (auto packed = dynamic_cast<PackedRecordType*>(ca->ty)) {
			if (packed->cxx_name.empty())
				unhandled_type("anonymous packed overlay", packed);
			fprintf(active, "([&]() { const auto& tpcc_overlay_source = ");
			emit_expression(ca->a);
			fprintf(active, "; ");
			fprintf(active, "using tpcc_overlay_source_type = std::remove_cvref_t<decltype(tpcc_overlay_source)>; ");
			fprintf(active, "static_assert(std::is_trivially_copyable_v<tpcc_overlay_source_type>, \"packed overlay source must be trivially copyable\"); ");
			fprintf(active, "static_assert(sizeof(tpcc_overlay_source_type) == %s::m_storage_size, \"packed overlay size mismatch\"); ",
				packed->cxx_name.c_str());
			fprintf(active, "%s tpcc_overlay_value{}; ", packed->cxx_name.c_str());
			fprintf(active, "std::memcpy(tpcc_overlay_value.m_data(), std::addressof(tpcc_overlay_source), sizeof(tpcc_overlay_source)); ");
			fprintf(active, "return tpcc_overlay_value; }())");
			return;
		}
		if (auto packed = dynamic_cast<PackedRecordType*>(ca->a ? ca->a->ty : nullptr)) {
			fprintf(active, "([&]() { const auto& tpcc_overlay_source = ");
			emit_expression(ca->a);
			fprintf(active, "; ");
			fprintf(active, "using tpcc_overlay_target_type = ");
			emit_type_ref(ca->ty);
			fprintf(active, "; ");
			fprintf(active, "static_assert(std::is_trivially_copyable_v<tpcc_overlay_target_type>, \"packed overlay target must be trivially copyable\"); ");
			fprintf(active, "static_assert(sizeof(tpcc_overlay_target_type) == %s::m_storage_size, \"packed overlay size mismatch\"); ",
				packed->cxx_name.c_str());
			fprintf(active, "tpcc_overlay_target_type tpcc_overlay_value{}; ");
			fprintf(active, "std::memcpy(std::addressof(tpcc_overlay_value), tpcc_overlay_source.m_data(), sizeof(tpcc_overlay_value)); ");
			fprintf(active, "return tpcc_overlay_value; }())");
			return;
		}
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
			if (dynamic_cast<AddrOf*>(u))
				emit_writable_expression(u->a);
			else
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
	if (auto r = dynamic_cast<PackedRecordType*>(ty)) {
		if (r->cxx_name.empty())
			unhandled_type("anonymous packed record type reference", ty);
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
		fprintf(active, "pas::t_set<");
		emit_type_ref(s->item_type);
		fprintf(active, ">");
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
