#include "emit.h"
#include "builtins.h"
#include "cst.h"
#include "frame.h"
#include "operators.h"
#include "types.h"
#include "units.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <set>
#include <typeinfo>

static std::string owner_cxx_name(Type* owner);
static std::string owner_cxx_reference_name(Type* owner);

/** Spell a reference to a semantic declaration. Source-defined unit members
 * carry a local C++ token plus their owning Unit; external/builtin/local
 * declarations carry no owner and retain their existing spelling verbatim.
 * Declaration sites do not use this helper because they are emitted inside
 * the owning namespace. */
static std::string owned_cxx_name(
    const Unit* owner, const std::string& local_name) {
	if (!owner)
		return local_name;
	return "::" + owner->cxx_namespace + "::" + local_name;
}

static std::string node_cxx_name(
    const Node* node, const std::string& local_name) {
	return owned_cxx_name(
	    node ? node->owning_unit : nullptr, local_name);
}

static std::string type_cxx_name(
    const Type* type, const std::string& local_name) {
	return owned_cxx_name(
	    type ? type->owning_unit : nullptr, local_name);
}

static std::string named_type_local_cxx_name(Type* type) {
	if (auto s = dynamic_cast<SubrangeType*>(type))
		return s->cxx_name;
	if (auto r = dynamic_cast<RecordType*>(type))
		return r->cxx_name;
	if (auto r = dynamic_cast<PackedRecordType*>(type))
		return r->cxx_name;
	if (auto c = dynamic_cast<ClassType*>(type))
		return c->cxx_name;
	if (auto c = dynamic_cast<ClassRefType*>(type))
		return c->cxx_name;
	if (auto i = dynamic_cast<InterfaceType*>(type))
		return i->cxx_name;
	if (auto o = dynamic_cast<ObjectType*>(type))
		return o->cxx_name;
	if (auto e = dynamic_cast<EnumType*>(type))
		return e->cxx_name;
	return "";
}

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
		fprintf(active, "%s ",
			e->cxx_name.c_str());
	// Fix the C++ underlying type to the EnumType carrier. An unfixed C++ enum
	// infers its value range from the listed enumerators, which would make a
	// Pascal explicit ordinal cast undefined to C++'s sanitizer even when the
	// value fits the compiler's declared representation.
	const char* underlying = nullptr;
	if (e->carrier_signed) {
		switch (e->carrier_bits) {
		case 8:
			underlying = "int8_t";
			break;
		case 16:
			underlying = "int16_t";
			break;
		case 32:
			underlying = "int32_t";
			break;
		case 64:
			underlying = "int64_t";
			break;
		}
	} else {
		switch (e->carrier_bits) {
		case 8:
			underlying = "uint8_t";
			break;
		case 16:
			underlying = "uint16_t";
			break;
		case 32:
			underlying = "uint32_t";
			break;
		case 64:
			underlying = "uint64_t";
			break;
		}
	}
	if (!underlying)
		unhandled_type(
		    "enum underlying carrier", e);
	fprintf(active, ": %s { ", underlying);
	for (size_t i = 0; i < e->members.size(); i++) {
		if (i)
			fprintf(active, ", ");
		const auto& member = e->members[i];
		fprintf(active, "%s", member.cxx_name.c_str());
		if (member.explicit_value)
			fprintf(active, " = %lld",
				static_cast<long long>(member.value));
	}
	fprintf(active, " }");
}

void Emitter::emit_static_member_declaration(
    StorageSlot* slot) {
	if (!slot ||
	    slot->kind !=
		StorageSlot::Kind::StaticMember)
		unhandled_node(
		    "non-static slot passed to static-member emitter",
		    slot);
	fprintf(active, "\tpublic: ");
	if (slot->initializer) {
		fprintf(active, "inline static ");
		emit_type_ref(slot->ty);
		fprintf(active, "& %s() {\n",
			slot->cxx_name.c_str());
		fprintf(active, "\t\tstatic ");
		emit_type_ref(slot->ty);
		fprintf(active, " m_value = ");
		emit_expression(slot->initializer);
		fprintf(active, ";\n");
		fprintf(active, "\t\treturn m_value;\n");
		fprintf(active, "\t}\n");
		return;
	}
	fprintf(active, "inline static ");
	emit_type_ref(slot->ty);
	fprintf(active, " %s{};\n",
		slot->cxx_name.c_str());
}

// Apply the backend prefix to a Pascal value identifier. Operator tokens
// receive `o_` because their C++ namespace must remain disjoint from an
// ordinary Pascal routine whose source name describes the same operation.
std::string cxx_value_name(std::string pas_name) {
	if (auto operator_name =
		legacy_operator_cxx_name(
		    pas_name))
		// Operator spellings live in the authoritative catalog because their
		// Pascal identity, metadata identity, and backend name must change
		// together. Ordinary identifiers alone receive the p_ namespace.
		return std::string(*operator_name);
	return "p_" + pas_name;
}

static Type* conversion_operator_target(
    const Callable* callable) {
	return callable &&
		       callable->is_conversion_operator()
		   ? callable->ty->return_type
		   : nullptr;
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
	active_unit_namespace.clear();
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
	active_unit_namespace.clear();
	emitted_subranges.clear();
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

void Emitter::emit_unit_interface_prologue(
    std::string unit_namespace,
    std::vector<std::string> used_unit_h_files) {
	if (!active)
		return;
	active_unit_namespace = unit_namespace;
	fprintf(active, "#pragma once\n");
	fprintf(active, "#include \"rtl.h\"\n");
	fprintf(active, "#include <functional>\n");
	for (auto& h : used_unit_h_files)
		fprintf(active, "#include \"%s\"\n", h.c_str());
	fprintf(active, "\nnamespace %s {\n",
		unit_namespace.c_str());
}

void Emitter::emit_unit_interface_epilogue() {
	if (!active)
		return;
	fprintf(active, "\n}\n");
}

void Emitter::emit_unit_implementation_prologue(
    std::string unit_namespace,
    std::string this_unit_h_file,
    std::vector<std::string> impl_used_unit_h_files) {
	if (!active)
		return;
	active_unit_namespace = unit_namespace;
	fprintf(active, "#include \"%s\"\n", this_unit_h_file.c_str());
	fprintf(active, "#include \"rtl.h\"\n");
	fprintf(active, "#include <functional>\n");
	for (auto& h : impl_used_unit_h_files)
		fprintf(active, "#include \"%s\"\n", h.c_str());
	fprintf(active, "\nnamespace %s {\n",
		unit_namespace.c_str());
}

void Emitter::emit_unit_implementation_epilogue() {
	if (!active)
		return;
	fprintf(active, "\n}\n");
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

void Emitter::emit_class_lifecycle_call(Method* method) {
	if (!active)
		return;
	if (!method ||
	    (method->ty->kind != CLASS_CONSTRUCTOR &&
	     method->ty->kind != CLASS_DESTRUCTOR) ||
	    !method->owner_class)
		unhandled_node(
		    "invalid class lifecycle entry", method);
	// The hook is deliberately invoked on the exact class's metaclass.
	// It is nonvirtual and is scheduled only for the class that declared it,
	// so C++ metaclass inheritance cannot accidentally run a parent hook for
	// a descendant that has no hook of its own.
	fprintf(active, "\t%s::p_classtype()->%s();\n",
		owner_cxx_reference_name(
		    method->owner_class)
		    .c_str(),
		method->cxx_name.c_str());
}

void Emitter::emit_subrange_definition(
    SubrangeType* subrange) {
	if (!active || !subrange ||
	    emitted_subranges.count(subrange))
		return;
	emitted_subranges.insert(subrange);

	// A range owned by another unit was declared in that unit's header. The
	// current output includes that header and must only qualify the existing
	// tag; emitting a second local definition would create a different type.
	if (subrange->owning_unit &&
	    subrange->owning_unit->cxx_namespace !=
		active_unit_namespace)
		return;

	fprintf(active, "struct %s {\n",
		subrange->cxx_name.c_str());
	fprintf(active,
		"\tusing m_tpcc_ordinal_storage_type = ");
	emit_type_ref(subrange->base_type);
	fprintf(active, ";\n");
	fprintf(active,
		"\tm_tpcc_ordinal_storage_type m_value{};\n");
	fprintf(active, "};\n");
	// Pascal layout and packed-record layout continue to use base_type. These
	// assertions make the one-member C++ representation an enforced contract
	// rather than relying on an assumed empty-padding optimization.
	fprintf(active,
		"static_assert(sizeof(%s) == sizeof(%s::m_tpcc_ordinal_storage_type), \"subrange carrier size mismatch\");\n",
		subrange->cxx_name.c_str(),
		subrange->cxx_name.c_str());
	fprintf(active,
		"static_assert(alignof(%s) == alignof(%s::m_tpcc_ordinal_storage_type), \"subrange carrier alignment mismatch\");\n",
		subrange->cxx_name.c_str(),
		subrange->cxx_name.c_str());
	fprintf(active,
		"static_assert(std::is_standard_layout_v<%s>, \"subrange carrier must have standard layout\");\n",
		subrange->cxx_name.c_str());
	fprintf(active,
		"static_assert(std::is_trivially_copyable_v<%s>, \"subrange carrier must be trivially copyable\");\n",
		subrange->cxx_name.c_str());
}

void Emitter::emit_type_dependencies(
    Type* root, bool inspect_definition) {
	if (!active || !root)
		return;
	std::set<std::pair<Type*, bool>> visited;
	std::function<void(Type*, bool)> visit;
	std::function<void(VariantPart*)> visit_variant;
	std::function<void(Frame*)> visit_frame;

	auto inspect_nested_definition =
	    [](Type* ty) {
		    return named_type_local_cxx_name(ty).empty();
	    };

	visit_variant = [&](VariantPart* variant) {
		if (!variant)
			return;
		visit(variant->selector_type,
		      inspect_nested_definition(
			  variant->selector_type));
		for (const auto& arm : variant->arms) {
			for (const auto& field : arm.fields)
				visit(field.ty,
				      inspect_nested_definition(
					  field.ty));
			visit_variant(arm.variant);
		}
	};

	visit_frame = [&](Frame* frame) {
		if (!frame)
			return;
		for (const auto& declaration :
		     frame->value_declarations()) {
			Node* value =
			    declaration.second.value;
			if (auto slot =
				dynamic_cast<StorageSlot*>(
				    value)) {
				visit(slot->ty,
				      inspect_nested_definition(
					  slot->ty));
				continue;
			}
			if (auto property =
				dynamic_cast<Property*>(
				    value)) {
				visit(property->ty,
				      inspect_nested_definition(
					  property->ty));
				for (Type* index :
				     property->index_types)
					visit(index,
					      inspect_nested_definition(
						  index));
				continue;
			}
			if (auto callable =
				dynamic_cast<Callable*>(
				    value)) {
				visit(callable->ty, false);
				continue;
			}
			if (auto overloads =
				dynamic_cast<OverloadSet*>(
				    value))
				for (Callable* callable :
				     overloads->members)
					visit(callable->ty,
					      false);
		}
	};

	visit = [&](Type* ty, bool inspect) {
		if (!ty ||
		    !visited.insert({ty, inspect}).second)
			return;
		if (auto incomplete =
			dynamic_cast<IncompleteType*>(ty)) {
			visit(incomplete->resolved, inspect);
			return;
		}
		if (auto subrange =
			dynamic_cast<SubrangeType*>(ty)) {
			visit(subrange->base_type, false);
			emit_subrange_definition(subrange);
			return;
		}
		if (auto array =
			dynamic_cast<FixedArrayType*>(ty)) {
			visit(array->bounds,
			      inspect_nested_definition(
				  array->bounds));
			visit(array->item_type,
			      inspect_nested_definition(
				  array->item_type));
			return;
		}
		if (auto array =
			dynamic_cast<DynamicArrayType*>(ty)) {
			visit(array->item_type,
			      inspect_nested_definition(
				  array->item_type));
			return;
		}
		if (auto array =
			dynamic_cast<OpenArrayType*>(ty)) {
			visit(array->item_type,
			      inspect_nested_definition(
				  array->item_type));
			return;
		}
		if (auto set =
			dynamic_cast<FixedSetType*>(ty)) {
			visit(set->item_type,
			      inspect_nested_definition(
				  set->item_type));
			return;
		}
		if (auto file =
			dynamic_cast<TypedFileType*>(ty)) {
			visit(file->item_type,
			      inspect_nested_definition(
				  file->item_type));
			return;
		}
		if (auto pointer =
			dynamic_cast<PointerType*>(ty)) {
			if (!pointer->is_untyped())
				visit(
				    pointer->item_type,
				    inspect_nested_definition(
					pointer->item_type));
			return;
		}
		if (auto routine =
			dynamic_cast<RoutineType*>(ty)) {
			visit(routine->return_type,
			      inspect_nested_definition(
				  routine->return_type));
			for (const auto& formal :
			     routine->formals)
				visit(
				    formal.ty,
				    inspect_nested_definition(
					formal.ty));
			return;
		}
		if (!inspect)
			return;
		if (auto record =
			dynamic_cast<RecordType*>(ty)) {
			for (const auto& field :
			     record->fields)
				visit(
				    field.ty,
				    inspect_nested_definition(
					field.ty));
			visit_variant(record->variant);
			visit_frame(record->children);
			return;
		}
		if (auto record =
			dynamic_cast<PackedRecordType*>(ty)) {
			visit_variant(record->variant);
			visit_frame(record->children);
			return;
		}
		if (auto record =
			dynamic_cast<ClassType*>(ty)) {
			visit_frame(record->children);
			return;
		}
		if (auto record =
			dynamic_cast<InterfaceType*>(ty)) {
			visit_frame(record->children);
			return;
		}
		if (auto record =
			dynamic_cast<ObjectType*>(ty))
			visit_frame(record->children);
	};
	visit(root, inspect_definition);
}

void Emitter::emit_var_decl(
    std::string cxx_name, Type* ty,
    Node* initializer) {
	if (!active)
		return;
	emit_type_dependencies(ty);
	// Pascal permits unused variables. C++'s unused-variable warning cannot
	// serve as a Pascal semantic check: it would reject a valid source program
	// whenever the generated C++ is compiled with -Werror.
	fprintf(active, "[[maybe_unused]] ");
	// A unit interface is emitted as a C++ header. C++20 inline variables
	// preserve Pascal's one unit-owned storage object while permitting that
	// definition in every translation unit which includes the interface.
	if (active == out_h)
		fprintf(active, "inline ");
	emit_type_ref(ty);
	fprintf(active, " %s", cxx_name.c_str());
	if (initializer) {
		fprintf(active, " = ");
		emit_expression(initializer);
	}
	fprintf(active, ";\n");
}

void Emitter::emit_initialized_storage_decl(
    std::string cxx_name, Type* ty,
    Node* initializer, bool routine_local) {
	if (!active)
		return;
	emit_type_dependencies(ty);
	// `const X: T = value` is Pascal initialized storage, not a true
	// compile-time constant. At routine scope that storage persists across
	// calls; at unit/program scope ordinary static storage duration already
	// supplies the same lifetime.
	fprintf(active, "[[maybe_unused]] ");
	if (routine_local)
		fprintf(active, "static ");
	else if (active == out_h)
		fprintf(active, "inline ");
	emit_type_ref(ty);
	fprintf(active, " %s = ", cxx_name.c_str());
	emit_expression(initializer);
	fprintf(active, ";\n");
}

void Emitter::emit_main_prologue(
    const std::vector<UnitLifecycleNames>&
	unit_lifecycle_hooks,
    const std::vector<Method*>&
	program_class_destructors) {
	if (!active)
		return;
	fprintf(active, "\n");
	for (const auto& unit : unit_lifecycle_hooks) {
		fprintf(active, "namespace %s {\n",
			unit.cxx_namespace.c_str());
		fprintf(active, "void %s();\n",
			unit.initialize.c_str());
		fprintf(active, "void %s();\n",
			unit.finalize.c_str());
		fprintf(active, "}\n");
	}
	fprintf(active, "\nnamespace {\n");
	fprintf(active, "struct tpcc_unit_entry {\n");
	fprintf(active, "\tvoid (*initialize)();\n");
	fprintf(active, "\tvoid (*finalize)();\n");
	fprintf(active, "};\n\n");
	fprintf(active,
		"constexpr std::array<tpcc_unit_entry, %zu> tpcc_units{{\n",
		unit_lifecycle_hooks.size());
	for (const auto& unit : unit_lifecycle_hooks)
		fprintf(active, "\t{::%s::%s, ::%s::%s},\n",
			unit.cxx_namespace.c_str(),
			unit.initialize.c_str(),
			unit.cxx_namespace.c_str(),
			unit.finalize.c_str());
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
	if (!program_class_destructors.empty()) {
		// Program lifecycle finalization must also run for Halt and normal
		// return, so it is an atexit callback rather than code pasted after
		// the program body. It is armed only after every program class
		// constructor succeeds. Since that registration happens after the
		// unit callback registration, C++'s reverse atexit order finalizes
		// the program before its units.
		fprintf(active,
			"bool tpcc_program_finalization_armed = false;\n");
		fprintf(active,
			"bool tpcc_program_finalization_started = false;\n\n");
		fprintf(active,
			"void tpcc_finalize_program() noexcept {\n");
		fprintf(active,
			"\tif (!tpcc_program_finalization_armed || "
			"tpcc_program_finalization_started)\n");
		fprintf(active, "\t\treturn;\n");
		fprintf(active,
			"\ttpcc_program_finalization_started = true;\n");
		for (Method* method : program_class_destructors)
			emit_class_lifecycle_call(method);
		fprintf(active, "}\n");
	}
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

void Emitter::emit_program_finalizer_registration() {
	if (!active)
		return;
	fprintf(active,
		"\t\ttpcc_program_finalization_armed = true;\n");
	fprintf(active,
		"\t\tif (std::atexit(tpcc_finalize_program) != 0) {\n");
	fprintf(active, "\t\t\ttpcc_finalize_program();\n");
	fprintf(active, "\t\t\tstd::terminate();\n");
	fprintf(active, "\t\t}\n");
}

void Emitter::emit_main_epilogue(
    bool has_program_class_destructors) {
	if (!active)
		return;
	fprintf(active, "\t\treturn 0;\n");
	fprintf(active,
		"\t} catch (::u_system::tpcc_pascal_exception<"
		"::u_system::t_tobject>& tpcc_exception) {\n"
		"\t\t::u_system::tpcc_pascal_exception_scope<"
		"::u_system::t_tobject> tpcc_exception_scope("
		"tpcc_exception);\n"
		"\t\tauto tpcc_exceptproc = "
		"::u_system::p_exceptproc;\n");
	if (has_program_class_destructors)
		fprintf(active, "\t\ttpcc_finalize_program();\n");
	fprintf(active, "\t\ttpcc_finalize_initialized_units();\n");
	fprintf(active,
		"\t\tif (tpcc_exceptproc)\n"
		"\t\t\ttpcc_exceptproc("
		"tpcc_exception.object(), "
		"tpcc_exception.address(), "
		"tpcc_exception.frame());\n"
		"\t\treturn 217;\n");
	fprintf(active, "\t} catch (...) {\n");
	if (has_program_class_destructors)
		fprintf(active, "\t\ttpcc_finalize_program();\n");
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

static std::string owner_cxx_reference_name(Type* owner) {
	return type_cxx_name(
	    owner, owner_cxx_name(owner));
}

// Only Pascal class destructors are presently implemented by C++ destructors.
// An old-style object is a value: its Pascal destructor is an ordinary,
// optionally virtual method, while Dispose separately tears down its carrier.
static bool callable_is_cxx_destructor(Callable* c) {
	if (!c || c->ty->kind != DESTRUCTOR)
		return false;
	auto method = dynamic_cast<Method*>(c);
	return method &&
	       dynamic_cast<ClassType*>(
		   method->owner_class);
}

// Spelling of a Callable's C++ name token at any emit site. For class
// destructors this is `~ClassName` so the token composes with `->` at the call site
// (obj->~Class()) and with `Owner::` for qualified destructor calls. Standard
// citations: [expr.prim.id.dtor] for the spelling, [expr.ref] for member
// access composition, [class.dtor]/15 for "A destructor can be called
// explicitly." For non-destructors, the value cxx name as-is.
static std::string callable_cxx_name(Callable* c) {
	if (callable_is_cxx_destructor(c)) {
		auto m = dynamic_cast<Method*>(c);
		return m && m->owner_class ? "~" + owner_cxx_name(m->owner_class) : "~";
	}
	return c->cxx_name;
}

static std::optional<std::string> variant_member_path(
    const VariantPart* variant, StorageSlot* slot) {
	if (!variant || !slot)
		return std::nullopt;
	if (variant->selector_slot == slot)
		return std::string{};
	for (size_t arm_index = 0;
	     arm_index < variant->arms.size(); ++arm_index) {
		const auto& arm = variant->arms[arm_index];
		const std::string prefix =
		    "m_variant.m_arm_" +
		    std::to_string(arm_index) + ".";
		for (const auto& field : arm.fields)
			if (field.slot == slot)
				return prefix;
		if (auto nested =
			variant_member_path(arm.variant, slot))
			return prefix + *nested;
	}
	return std::nullopt;
}

static std::optional<std::string>
record_variant_path(Type* owner, StorageSlot* slot) {
	auto record = dynamic_cast<RecordType*>(owner);
	if (!record || !slot)
		return std::nullopt;
	return variant_member_path(record->variant, slot);
}

static std::string variant_arm_type_name(
    unsigned depth, size_t arm_index) {
	// A nested arm type is declared inside its containing arm type. Reusing
	// the same name there (for example, arm 1 nested in arm 1) would give a
	// C++ member class the name of its enclosing class, which is reserved for
	// constructors. The depth component makes every nested helper distinct;
	// depth zero retains the original, readable spelling.
	return depth == 0
		   ? "m_variant_arm_" + std::to_string(arm_index) + "_type"
		   : "m_variant_" + std::to_string(depth) + "_arm_" +
			 std::to_string(arm_index) + "_type";
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

void Emitter::emit_try_prologue() {
	if (!active)
		return;
	fprintf(active,
		"\ttry {\n"
		"\t\t[[maybe_unused]] std::exception_ptr "
		"tpcc_pending_exception;\n"
		"\t\ttry {\n");
}

void Emitter::emit_try_except_prologue() {
	if (!active)
		return;
	// Pascal except catches only the Pascal carrier. Constructor Fail,
	// native implementation failures, and compiler-private control transfer
	// therefore pass through without name-specific rethrow branches.
	fprintf(active,
		"\t\t} catch "
		"(::u_system::tpcc_pascal_exception<"
		"::u_system::t_tobject>& tpcc_exception) {\n"
		"\t\t\t::u_system::tpcc_pascal_exception_scope<"
		"::u_system::t_tobject> tpcc_exception_scope("
		"tpcc_exception);\n");
}

void Emitter::emit_exception_handler_prologue(
    Type* exception_type, std::string variable_cxx_name,
    bool first) {
	if (!active)
		return;
	auto exception_class =
	    dynamic_cast<ClassType*>(exception_type);
	if (!exception_class)
		unhandled_type(
		    "exception handler type is not a class",
		    exception_type);
	fprintf(active, "\t\t\t%sif (",
		first ? "" : "else ");
	if (variable_cxx_name.empty()) {
		fprintf(active,
			"tpcc_exception.get_if<%s>() != nullptr",
			owner_cxx_reference_name(
			    exception_class)
			    .c_str());
	} else {
		fprintf(active,
			"auto* %s = tpcc_exception.get_if<%s>()",
			variable_cxx_name.c_str(),
			owner_cxx_reference_name(
			    exception_class)
			    .c_str());
	}
	fprintf(active, ") {\n");
}

void Emitter::emit_exception_handler_epilogue() {
	if (active)
		fprintf(active, "\t\t\t}\n");
}

void Emitter::emit_exception_default_prologue() {
	if (active)
		fprintf(active, "\t\t\telse {\n");
}

void Emitter::emit_exception_default_epilogue() {
	if (active)
		fprintf(active, "\t\t\t}\n");
}

void Emitter::emit_try_except_epilogue(
    bool typed_handlers, bool has_default) {
	if (!active)
		return;
	if (typed_handlers && !has_default)
		fprintf(active,
			"\t\t\telse {\n"
			"\t\t\t\tthrow;\n"
			"\t\t\t}\n");
	fprintf(active, "\t\t}\n");
}

void Emitter::emit_try_finally_prologue() {
	if (!active)
		return;
	// Save an arbitrary pending unwind opaquely. The finally statements
	// stream once after the catch on both normal and exceptional completion.
	// No Pascal handler examines or converts this exception_ptr.
	fprintf(active,
		"\t\t} catch (...) {\n"
		"\t\t\ttpcc_pending_exception = "
		"std::current_exception();\n"
		"\t\t}\n");
}

void Emitter::emit_try_finally_epilogue() {
	if (!active)
		return;
	fprintf(active,
		"\t\tif (tpcc_pending_exception)\n"
		"\t\t\tstd::rethrow_exception("
		"tpcc_pending_exception);\n");
}

void Emitter::emit_return_transfer_handler(
    unsigned try_depth, RoutineType* routine) {
	if (!routine)
		return;
	fprintf(active,
		" catch (::u_system::tpcc_return_transfer<");
	emit_type_ref(routine->return_type);
	fprintf(active,
		">& tpcc_return) {\n"
		"\t\tif (tpcc_return.next_try_depth != %u)\n"
		"\t\t\tthrow;\n"
		"\t\t--tpcc_return.next_try_depth;\n"
		"\t\tif (tpcc_return.next_try_depth != 0)\n"
		"\t\t\tthrow;\n",
		try_depth);
	if (routine->return_type == &unit_type())
		fprintf(active, "\t\treturn;\n");
	else
		fprintf(active,
			"\t\treturn std::move("
			"tpcc_return.value);\n");
	fprintf(active, "\t}");
}

void Emitter::emit_try_control_epilogue(
    unsigned try_depth, RoutineType* routine,
    bool inside_loop) {
	if (!active)
		return;
	fprintf(active, "\t}");
	emit_return_transfer_handler(
	    try_depth, routine);
	if (inside_loop) {
		fprintf(active,
			" catch (::u_system::tpcc_loop_transfer& "
			"tpcc_loop) {\n"
			"\t\tif (tpcc_loop.next_try_depth != %u)\n"
			"\t\t\tthrow;\n"
			"\t\t--tpcc_loop.next_try_depth;\n"
			"\t\tif (tpcc_loop.next_try_depth != "
			"tpcc_loop.target_try_depth)\n"
			"\t\t\tthrow;\n"
			"\t\tif (tpcc_loop.is_break)\n"
			"\t\t\tbreak;\n"
			"\t\tcontinue;\n"
			"\t}",
			try_depth);
	}
	if (!routine && !inside_loop)
		fprintf(active,
			" catch (...) {\n"
			"\t\tthrow;\n"
			"\t}");
	fprintf(active, "\n");
}

void Emitter::emit_for_in_cleanup_control_epilogue(
    unsigned try_depth, RoutineType* routine) {
	if (!active)
		return;
	fprintf(active, "\t}");
	emit_return_transfer_handler(
	    try_depth, routine);
	// This generated try contains the while loop. A break reaching its target
	// has already left that while, so consuming the carrier completes the
	// source break. Continue targets the still-active loop depth and should
	// never unwind as far as this boundary.
	fprintf(active,
		" catch (::u_system::tpcc_loop_transfer& "
		"tpcc_loop) {\n"
		"\t\tif (tpcc_loop.next_try_depth != %u)\n"
		"\t\t\tthrow;\n"
		"\t\t--tpcc_loop.next_try_depth;\n"
		"\t\tif (tpcc_loop.next_try_depth != "
		"tpcc_loop.target_try_depth)\n"
		"\t\t\tthrow;\n"
		"\t\tif (!tpcc_loop.is_break)\n"
		"\t\t\tthrow;\n"
		"\t}\n",
		try_depth);
}

void Emitter::emit_formatted_value(
    const FormattedValue& formatted) {
	fprintf(
	    active,
	    "::u_system::tpcc_make_formatted_value("
	    "static_cast<");
	emit_type_ref(formatted.value->ty);
	fprintf(active, ">(");
	emit_expression(formatted.value);
	fprintf(active, ")");
	if (formatted.width) {
		fprintf(
		    active,
		    ", static_cast<::u_system::t_sizeint>(");
		emit_expression(formatted.width);
		fprintf(active, ")");
	}
	if (formatted.precision) {
		fprintf(
		    active,
		    ", static_cast<::u_system::t_sizeint>(");
		emit_expression(formatted.precision);
		fprintf(active, ")");
	}
	fprintf(active, ")");
}

void Emitter::emit_statement(Node* stmt) {
	if (!active)
		return;
	if (dynamic_cast<ConstructorFail*>(stmt)) {
		fprintf(active,
			"\tthrow ::u_system::tpcc_constructor_fail{};\n");
		return;
	}
	if (auto raise = dynamic_cast<Raise*>(stmt)) {
		if (!raise->object) {
			fprintf(active, "\tthrow;\n");
			return;
		}
		fprintf(active,
			"\t::u_system::m_raise_pascal(");
		emit_expression(raise->object);
		if (raise->address) {
			fprintf(active, ", ");
			emit_expression(raise->address);
			fprintf(active, ", ");
			if (raise->frame)
				emit_expression(raise->frame);
			else
				fprintf(active, "nullptr");
		}
		fprintf(active, ");\n");
		return;
	}
	if (auto mutation =
		dynamic_cast<Mutation*>(stmt)) {
		// The aliases make every runtime component of the Pascal place stable
		// before its getter/read runs. The final store remains an ordinary
		// Assign node on purpose: its existing property setters, packed
		// copyback, writable-cast storage, and range-checked conversion are
		// the language's assignment semantics and must not be duplicated by
		// Inc/Dec.
		fprintf(active, "\t[&]() {\n");
		for (const Mutation::Binding& binding :
		     mutation->bindings) {
			fprintf(active, "\t\tauto&& %s = ",
				binding.alias->cxx_name.c_str());
			emit_expression(
			    binding.initializer);
			fprintf(active, ";\n");
		}
		fprintf(active,
			"\t\tauto %s = ",
			mutation->current
			    ->cxx_name.c_str());
		emit_expression(
		    mutation->target);
		fprintf(active, ";\n");
		fprintf(active, "\t");
		emit_statement(
		    mutation->assignment);
		fprintf(active, "\t}();\n");
		return;
	}
	if (auto a = dynamic_cast<Assign*>(stmt)) {
		if (auto member =
			dynamic_cast<MemberAccess*>(a->a)) {
			if (auto view =
				dynamic_cast<Cast*>(member->a)) {
				auto field =
				    dynamic_cast<StorageSlot*>(member->b);
				auto routine =
				    dynamic_cast<RoutineType*>(
					view->a ? view->a->ty : nullptr);
				bool is_code =
				    field == tmethod_code_field();
				bool is_data =
				    field == tmethod_data_field();
				if (view->ty == tmethod_type() &&
				    field && routine &&
				    routine->kind == METHOD &&
				    (is_code || is_data)) {
					fprintf(active,
						is_code
						    ? "\t::u_system::m_store_tmethod_code("
						    : "\t::u_system::m_store_tmethod_data(");
					emit_writable_expression(view->a);
					fprintf(active, ", ");
					emit_expression(a->b);
					fprintf(active, ");\n");
					return;
				}
			}
		}
		if (auto cast = dynamic_cast<Cast*>(a->a)) {
			// An assignable explicit ordinal cast is a same-sized view of an
			// existing Pascal place. Keep it out of ordinary C++ cast syntax:
			// a static_cast expression is not an lvalue, and reinterpret_cast
			// would create aliasing/lifetime hazards. The RTL helper bit-copies
			// the target value into a real source-carrier value, then assigns
			// that value through the typed storage view.
			fprintf(active, "\t::u_system::tpcc_store_writable_cast<");
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
						const char* index_name =
						    "::u_system::p_index";
						if (indexed_property) {
							auto builtin =
							    dynamic_cast<Builtin*>(
								indexed_property
								    ->property
								    ->write_accessor);
							if (builtin &&
							    builtin->desc &&
							    builtin->desc->cxx_name ==
								"::u_system::m_unchecked_index")
								index_name =
								    "::u_system::m_unchecked_index";
						}
						fprintf(active, "\t\t%s(tpcc_overlay_field, ", index_name);
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
		if (!write->lowering_builtin_desc)
			unhandled_node(
			    "Write/WriteLn has no selected RTL implementation",
			    write);
		fprintf(
		    active, "\t%.*s(",
		    static_cast<int>(
			write->lowering_builtin_desc
			    ->cxx_name.size()),
		    write->lowering_builtin_desc
			->cxx_name.data());
		bool need_comma = false;
		if (write->file) {
			emit_writable_expression(write->file);
			need_comma = true;
		}
		for (const FormattedValue& item : write->items) {
			if (need_comma)
				fprintf(active, ", ");
			emit_formatted_value(item);
			need_comma = true;
		}
		fprintf(active, ");\n");
		return;
	}
	if (auto str = dynamic_cast<StrCall*>(stmt)) {
		if (str->formatted.precision)
			// The parser currently rejects this branch. Keeping the CST edge
			// and an explicit backend guard makes the future real formatter a
			// new lowering contract rather than silently accepting an
			// unsupported source family.
			unhandled_node(
			    "Str precision lowering is not implemented",
			    str);
		fprintf(active, "\t::u_system::p_str(");
		emit_formatted_value(
		    str->formatted);
		fprintf(active, ", ");
		emit_writable_expression(
		    str->destination);
		fprintf(active, ");\n");
		return;
	}
	if (auto val = dynamic_cast<ValCall*>(stmt)) {
		fprintf(active, "\t::u_system::p_val(");
		emit_expression(val->source);
		fprintf(active, ", ");
		// Val is compiler-owned specifically so the selected destination
		// reaches the RTL as its exact C++ carrier. An ordinary ProcCall would
		// instead emit the omitted Pascal formal as an opaque storage view.
		emit_writable_expression(
		    val->destination);
		if (val->code) {
			fprintf(active, ", ");
			if (val->code->ty ==
			    integer_type())
				emit_writable_expression(
				    val->code);
			else
				// Non-Integer ordinal carriers use the typed storage-view
				// overload; the RTL writes the parsed Integer code back
				// through that carrier without var-parameter covariance.
				emit_storage_ref(
				    val->code);
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
		if (r->try_depth != 0) {
			fprintf(active,
				"\tthrow ::u_system::tpcc_return_transfer<");
			emit_type_ref(
			    r->a ? r->a->ty : &unit_type());
			fprintf(active, ">{%u", r->try_depth);
			if (r->a) {
				fprintf(active, ", ");
				emit_expression(r->a);
			}
			fprintf(active, "};\n");
		} else {
			fprintf(active, "\treturn");
			if (r->a) {
				fprintf(active, " ");
				emit_expression(r->a);
			}
			fprintf(active, ";\n");
		}
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
			owner_cxx_reference_name(
			    m->owner_class)
			    .c_str(),
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

void Emitter::emit_for_prologue(
    Node* control, Node* initial,
    Node* final, bool descending,
    bool overflow_checks) {
	if (!active)
		return;
	// Snapshot both bounds once. tpcc_for_done prevents the artificial step
	// after the terminal iteration from overflowing at High(T)/Low(T). Any
	// actual generated step still uses the for-statement's {$Q} state, exactly
	// like source Succ/Pred; keeping it in the C++ increment expression also
	// gives Pascal Continue its proper "perform the step, then retest"
	// behavior.
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
	fprintf(active, " = tpcc_for_initial; !tpcc_for_done && ::u_system::tpcc_for_%s_equal(",
		descending ? "greater" : "less");
	emit_expression(control);
	fprintf(active, ", tpcc_for_final); tpcc_for_done = ::u_system::tpcc_for_equal(");
	emit_expression(control);
	fprintf(active, ", tpcc_for_final), ");
	emit_expression(control);
	fprintf(active, " = tpcc_for_done ? ");
	emit_expression(control);
	fprintf(
	    active, " : ::u_system::%s%s(",
	    overflow_checks ? "p_" : "m_unchecked_",
	    descending ? "pred" : "succ");
	emit_expression(control);
	fprintf(active, ")) {\n");
}

void Emitter::emit_for_epilogue() {
	if (!active)
		return;
	fprintf(active, "\t}\n");
	fprintf(active, "\t}\n");
}

static void emit_for_in_loop_open(
    Emitter* emitter, FILE* active,
    Node* current_assignment) {
	// Every builtin iteration family has the same observable loop protocol.
	// Only adapter construction differs, so keeping the move/current loop
	// here prevents sequence, set, and ordinal paths from drifting apart.
	fprintf(active,
		"\twhile (tpcc_for_enumerator.m_move_next()) {\n");
	emitter->emit_statement(
	    current_assignment);
}

void Emitter::emit_for_in_sequence_prologue(
    Node* collection, Node* current_assignment) {
	if (!active)
		return;
	fprintf(active, "\t{\n");
	// Pascal evaluates the collection expression once. `auto&&` also extends
	// the lifetime of a temporary through the loop; m_enumerate then chooses
	// whether a view is sufficient or an owning handle must pin its storage.
	fprintf(active,
		"\tauto&& tpcc_for_collection = ");
	emit_expression(collection);
	fprintf(active, ";\n");
	fprintf(active,
		"\tauto tpcc_for_enumerator = "
		"::u_system::m_enumerate("
		"tpcc_for_collection);\n");
	emit_for_in_loop_open(
	    this, active, current_assignment);
}

void Emitter::emit_for_in_set_prologue(
    Node* collection, Node* lower,
    Node* upper, Node* current_assignment) {
	if (!active)
		return;
	fprintf(active, "\t{\n");
	fprintf(active,
		"\tauto&& tpcc_for_collection = ");
	emit_expression(collection);
	fprintf(active, ";\n");
	fprintf(active,
		"\tauto tpcc_for_enumerator = "
		"::u_system::m_enumerate("
		"tpcc_for_collection, ");
	// t_set stores normalized spans, not its Pascal declaration bounds. Pass
	// the semantic item domain explicitly so iteration neither escapes a
	// subrange nor depends on the carrier's internal representation.
	auto set_type =
	    dynamic_cast<FixedSetType*>(
		collection ? collection->ty : nullptr);
	if (!set_type)
		unhandled_node(
		    "for-in set source has non-set type",
		    collection);
	fprintf(active, "static_cast<");
	emit_type_ref(set_type->item_type);
	fprintf(active, ">(");
	emit_expression(lower);
	fprintf(active, "), static_cast<");
	emit_type_ref(set_type->item_type);
	fprintf(active, ">(");
	emit_expression(upper);
	fprintf(active, "));\n");
	emit_for_in_loop_open(
	    this, active, current_assignment);
}

void Emitter::emit_for_in_ordinal_prologue(
    Type* ordinal_type, Node* lower,
    Node* upper, Node* current_assignment) {
	if (!active)
		return;
	fprintf(active, "\t{\n");
	fprintf(active,
		"\tauto tpcc_for_enumerator = "
		"::u_system::m_enumerate("
		"::u_system::m_ordinal_range<");
	emit_type_ref(ordinal_type);
	fprintf(active, ">{static_cast<");
	emit_type_ref(ordinal_type);
	fprintf(active, ">(");
	emit_expression(lower);
	fprintf(active, "), static_cast<");
	emit_type_ref(ordinal_type);
	fprintf(active, ">(");
	emit_expression(upper);
	fprintf(active, ")});\n");
	emit_for_in_loop_open(
	    this, active, current_assignment);
}

void Emitter::emit_for_in_custom_setup(
    Node* get_enumerator, bool nullable) {
	if (!active)
		return;
	fprintf(active, "\t{\n");
	// GetEnumerator is an ordinary Pascal call, but its exact result needs a
	// stable C++ local so MoveNext, Current, and cleanup all share one value.
	fprintf(active,
		"\tauto tpcc_for_enumerator = ");
	emit_expression(get_enumerator);
	fprintf(active, ";\n");
	if (nullable)
		// A class enumerator may be nil. Keep both the loop and its cleanup
		// inside the test so nil means an empty iteration without a member
		// invocation on the null reference.
		fprintf(active,
			"\tif (tpcc_for_enumerator != nullptr) {\n");
}

void Emitter::emit_for_in_custom_loop_prologue(
    Node* move_next, Node* current_assignment) {
	if (!active)
		return;
	fprintf(active, "\twhile (");
	// MoveNext was already selected by the ordinary Pascal call resolver.
	// Emission applies that exact node instead of rediscovering a C++ member.
	emit_expression(move_next);
	fprintf(active, ") {\n");
	emit_statement(current_assignment);
}

void Emitter::emit_for_in_loop_epilogue() {
	if (active)
		fprintf(active, "\t}\n");
}

void Emitter::emit_for_in_custom_epilogue(
    bool nullable) {
	if (!active)
		return;
	if (nullable)
		fprintf(active, "\t}\n");
	fprintf(active, "\t}\n");
}

void Emitter::emit_for_in_epilogue() {
	if (!active)
		return;
	emit_for_in_loop_epilogue();
	fprintf(active, "\t}\n");
}

void Emitter::emit_loop_control(
    bool is_break, unsigned try_depth,
    unsigned target_try_depth) {
	if (!active)
		return;
	if (try_depth == target_try_depth) {
		fprintf(active,
			is_break ? "\tbreak;\n" : "\tcontinue;\n");
		return;
	}
	fprintf(active,
		"\tthrow ::u_system::tpcc_loop_transfer{%u, %u, %s};\n",
		try_depth, target_try_depth,
		is_break ? "true" : "false");
}

void Emitter::emit_formal_parameter(
    const Parameter& formal, bool with_name) {
	if (auto open =
		dynamic_cast<OpenArrayType*>(
		    formal.ty)) {
		// Every mode passes one two-word view by value. Constness belongs to
		// the viewed elements; a C++ reference here would instead refer to the
		// temporary descriptor and would not model Pascal var/out element
		// access.
		fprintf(active, "::u_system::t_openarray<");
		if (formal.mode == ParamMode::Const)
			fprintf(active, "const ");
		emit_type_ref(open->item_type);
		fprintf(active, ">");
	} else if (formal.ty == unknown_type() &&
		   (formal.mode == ParamMode::Var ||
		    formal.mode == ParamMode::Out ||
		    formal.mode == ParamMode::Const)) {
		fprintf(active, formal.mode == ParamMode::Const
				    ? "::u_system::tpcc_const_storage_ref"
				    : "::u_system::tpcc_storage_ref");
	} else {
		if (formal.mode == ParamMode::Const)
			fprintf(active, "const ");
		emit_type_ref(formal.ty);
		if (formal.mode == ParamMode::Var ||
		    formal.mode == ParamMode::Out ||
		    formal.mode == ParamMode::Const)
			fprintf(active, "&");
	}
	if (with_name)
		fprintf(active, " %s", formal.cxx_name.c_str());
}

void Emitter::emit_formal_parameters(
    RoutineType* ty, bool with_names,
    Type* conversion_target) {
	fprintf(active, "(");
	for (size_t i = 0; i < ty->formals.size(); i++) {
		if (i > 0)
			fprintf(active, ", ");
		emit_formal_parameter(ty->formals[i], with_names);
	}
	if (conversion_target) {
		if (!ty->formals.empty())
			fprintf(active, ", ");
		fprintf(active,
			"::u_system::m_conversion_target<");
		emit_type_ref(conversion_target);
		fprintf(active, ">");
	}
	fprintf(active, ")");
}

void Emitter::emit_function_type(RoutineType* ty) {
	emit_type_ref(ty->return_type);
	emit_formal_parameters(
	    ty, false, nullptr);
}

void Emitter::emit_routine_signature(
    RoutineType* ty, std::string cxx_text,
    Position pos, std::string owner_qualifier,
    bool cxx_destructor,
    Type* conversion_target) {
	if (!active)
		return;
	if (pos == Position::DeclarationFormalsOnly) {
		emit_formal_parameters(
		    ty, true, conversion_target);
		return;
	}
	if (!cxx_destructor) {
		emit_type_ref(ty->return_type);
		fprintf(active, " ");
	} else if (ty->return_type != &unit_type()) {
		unhandled_type("non-unit return type on destructor is not allowed", ty);
	}
	fprintf(active, "%s%s", owner_qualifier.c_str(), cxx_text.c_str());
	emit_formal_parameters(
	    ty, true, conversion_target);
}

void Emitter::emit_callable_signature(Callable* c, Position pos, std::string owner_qualifier) {
	emit_routine_signature(
	    c->ty, callable_cxx_name(c), pos,
	    owner_qualifier,
	    callable_is_cxx_destructor(c),
	    conversion_operator_target(c));
}

void Emitter::emit_procedure_open(Callable* c, bool nested_lambda) {
	if (!active)
		return;
	emit_type_dependencies(c ? c->ty : nullptr);
	fprintf(active, "\n");
	if (nested_lambda) {
		fprintf(active, "\tauto %s = [&]", callable_cxx_name(c).c_str());
		emit_routine_signature(
		    c->ty, "",
		    Position::DeclarationFormalsOnly,
		    "", false,
		    conversion_operator_target(c));
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
			if (c->ty->kind == CLASS_METHOD ||
			    c->ty->kind == CLASS_CONSTRUCTOR ||
			    c->ty->kind == CLASS_DESTRUCTOR)
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
	if (ty->return_type != &unit_type()) {
		fprintf(active, "\treturn p_result;\n");
	}
	fprintf(active, nested_lambda ? "\t};\n" : "}\n");
}

void Emitter::emit_callable_prototype(Callable* c, std::string owner_qualifier, std::string prefix, std::string suffix) {
	if (!active)
		return;
	emit_type_dependencies(c ? c->ty : nullptr);
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
			auto super_cxx_name =
			    type_cxx_name(
				c->super,
				c->super->cxx_name);
			if (in_meta) {
				super_cxx_name = super_cxx_name + "::m_meta";
			}
			fprintf(active, " : public %s", super_cxx_name.c_str());
			first = false;
		}
		if (in_meta) {
			if (c->cxx_name.empty())
				unhandled_type(
				    "metaclass marker target has no emitted type binding",
				    c);
			fprintf(active,
				first
				    ? " : public ::u_system::m_classref<%s>"
				    : ", public ::u_system::m_classref<%s>",
				type_cxx_name(
				    c, c->cxx_name)
				    .c_str());
			first = false;
		}
		if (!in_meta) {
			for (auto interface_type : c->implemented_interfaces) {
				fprintf(active, first ? " : public %s" : ", public %s",
					type_cxx_name(
					    interface_type,
					    interface_type->cxx_name)
					    .c_str());
				first = false;
			}
		} else {
			// not sure. TODO: m_iobject ?
		}
	} else if (auto c = dynamic_cast<InterfaceType*>(ty)) {
		bool first = true;
		for (auto interface_type : c->super_interfaces) {
			fprintf(active, first ? " : public %s" : ", public %s",
				type_cxx_name(
				    interface_type,
				    interface_type->cxx_name)
				    .c_str());
			first = false;
		}
	} else if (auto c = dynamic_cast<ObjectType*>(ty)) {
		if (c->super)
			fprintf(active, " : public %s",
				type_cxx_name(
				    c->super,
				    c->super->cxx_name)
				    .c_str());
	}

	fprintf(active, " {\n");
	if (auto object =
		dynamic_cast<ObjectType*>(ty)) {
		if (object->needs_vmt &&
		    (!object->super ||
		     !object->super->needs_vmt)) {
			if (cxx_name.empty())
				unhandled_type(
				    "VMT-bearing anonymous old-style "
				    "object has no C++ carrier binding",
				    object);
			// This destructor is not a Pascal `destructor Done`. It is
			// carrier machinery only: it supplies the native vptr when
			// needed and lets Dispose delete the exact derived carrier
			// after the Pascal Done method has returned.
			fprintf(active,
				"\tpublic: virtual ~%s() = default;\n",
				cxx_name.c_str());
		}
	}
	auto classref_api_cxx_for = [&](ClassType* c) -> std::string {
		ClassType* target = c;
		while (target->super)
			target = target->super;
		if (target->cxx_name.empty())
			unhandled_type("metaclass API target name unknown", target);
		return "::u_system::m_classref<" +
		       type_cxx_name(
			   target, target->cxx_name) +
		       ">*";
	};
	if (is_class && in_meta) {
		if (auto c = dynamic_cast<ClassType*>(ty)) {
			std::string class_name = c->cxx_name; // FIXME: terrible name.
			std::string parent_class_cxx_name =
			    c->super
				? type_cxx_name(
				      c->super,
				      c->super->cxx_name)
				: ""; // FIXME: terrible name
			if (c->super && parent_class_cxx_name.empty()) {
				unhandled_type("parent class name unknown", c);
			}
			std::string classref_api_cxx = classref_api_cxx_for(c);
			if (!c->super) {
				// Pascal class methods have a metaclass Self. ClassType is
				// the root operation that exposes that same receiver to
				// Pascal, so its implementation belongs here and returns
				// `this`. Derived metaclasses inherit the virtual operation;
				// making it a C++ static function would lose the dynamic
				// class-reference receiver.
				fprintf(active,
					"\tpublic: virtual inline %s p_classtype() {\n",
					classref_api_cxx.c_str());
				fprintf(active, "\t\treturn this;\n");
				fprintf(active, "\t}\n");
			}
			if (!body->declares_value("classname")) {
				fprintf(active, "\tpublic: virtual inline ::u_system::t_shortstring<255> p_classname() {\n");
				fprintf(active,
					"\t\treturn ::u_system::tpcc_shortstring_from_c(\"%s\", strlen(\"%s\"));\n",
					class_name.c_str(), class_name.c_str()); // FIXME: escape
				fprintf(active, "\t}\n");
			}
			if (!body->declares_value("instancesize")) {
				fprintf(active, "\tpublic: virtual inline ::u_system::t_sizeint p_instancesize() {\n");
				fprintf(active,
					"\t\treturn sizeof(%s);\n", class_name.c_str()); // TODO: namespace::super
				fprintf(active, "\t}\n");
			}
			if (!body->declares_value("inheritsfrom")) {
				fprintf(active, "\tpublic: virtual inline ::u_system::t_boolean p_inheritsfrom(%s s) {\n", classref_api_cxx.c_str());
				if (parent_class_cxx_name.empty()) {
					fprintf(active, "\t\treturn ::u_system::tpcc_bool_to_boolean(s == this);\n");
				} else {
					fprintf(active, "\t\treturn ::u_system::tpcc_bool_to_boolean(s == this || %s::m_meta::p_inheritsfrom(s));\n", parent_class_cxx_name.c_str());
				}
				fprintf(active, "\t}\n");
			}
			if (!body->declares_value("classparent")) {
				fprintf(active, "\tpublic: virtual inline %s p_classparent() {\n", classref_api_cxx.c_str());
				if (parent_class_cxx_name.empty()) {
					fprintf(active, "\t\treturn nullptr;\n");
				} else {
					fprintf(active, "\t\treturn %s::p_classtype();\n", parent_class_cxx_name.c_str());
				}
				fprintf(active, "\t}\n");
			}
			// Allocation is virtual on the metaclass so an inherited
			// NewInstance implementation still creates the exact class
			// represented by its receiver. The out-of-class definition is
			// emitted only after the outer object type is complete.
			std::string object_cxx_name =
			    type_cxx_name(c, c->cxx_name);
			fprintf(active, c->super ? "\tpublic: %s* m_allocate() override;\n" : "\tpublic: virtual %s* m_allocate();\n",
				object_cxx_name.c_str());
			if (c->class_constructor) {
				fprintf(active, "\t");
				emit_callable_signature(
				    c->class_constructor,
				    Position::Declaration, "");
				fprintf(active, ";\n");
			}
			if (c->class_destructor) {
				fprintf(active, "\t");
				emit_callable_signature(
				    c->class_destructor,
				    Position::Declaration, "");
				fprintf(active, ";\n");
			}
			// fallthrough
		} else {
			unhandled_type("emit_aggregate_decl", ty);
		}
	} else if (is_class && !in_meta) {
		auto c = static_cast<ClassType*>(ty);
		emit_aggregate_decl("m_meta", ty, true);
		fprintf(active, ";\n");
		// This is the one stable class-reference accessor. It owns the exact
		// metaclass object for T. Class methods are intentionally not mirrored
		// as outer C++ static proxies: such proxies have no metaclass `this`
		// and therefore cannot preserve derived class-method dispatch.
		fprintf(active,
			"\tpublic: inline static m_meta* p_classtype() {\n");
		fprintf(active, "\t\tstatic m_meta meta{};\n");
		fprintf(active, "\t\treturn &meta;\n");
		fprintf(active, "\t}\n");
		// A class name already has an exact class-reference value through the
		// static accessor above. An object expression needs the inverse
		// operation: recover the metaclass of its dynamic object. C++ virtual
		// dispatch supplies that information without storing another pointer
		// in every object. The return override is covariant because generated
		// metaclasses inherit in the same order as their object classes.
		fprintf(active, c->super
				    ? "\tpublic: inline m_meta* m_classref() override {\n"
				    : "\tpublic: virtual inline m_meta* m_classref() {\n");
		fprintf(active, "\t\treturn p_classtype();\n");
		fprintf(active, "\t}\n");
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
	//   Every slot is registered in the SAME Frame so Pascal lookup remains
	//   flat. RecordType::fields and VariantPart retain only the source-order
	//   layout tree; the generic frame walk must not emit record fields again.
	if (rec) {
		for (const auto& field : rec->fields) {
			fprintf(active, "\t");
			emit_type_ref(field.ty);
			fprintf(active, " %s;\n", field.slot->cxx_name.c_str());
		}
	}
	for (auto& kv : body->value_declarations()) {
		Node* v = kv.second.value;
		if (auto slot = dynamic_cast<StorageSlot*>(v)) {
			if (slot->kind ==
			    StorageSlot::Kind::StaticMember) {
				if (!(is_class && in_meta))
					emit_static_member_declaration(
					    slot);
				continue;
			}
			if (is_interface) {
				unhandled_type("emit_aggregate_decl interfaces cannot have variables", ty);
				continue;
			}
			if (rec)
				continue;
			if (is_class && in_meta)
				continue;
			fprintf(active, "\t");
			emit_type_ref(slot->ty);
			fprintf(active, " %s", slot->cxx_name.c_str());
			fprintf(active, ";\n");
		} else {
			std::vector<Callable*> callables;
			if (auto call = dynamic_cast<Callable*>(v))
				callables.push_back(call);
			else if (auto overloads =
				     dynamic_cast<OverloadSet*>(v))
				callables = overloads->members;
			for (Callable* call : callables) {
				// One Pascal class frame feeds two C++ carriers. Flatten
				// overload sets here, then route each declaration by its
				// already-known RoutineKind. The outer class never receives
				// a static class-method proxy: a static proxy has no
				// metaclass Self and cannot preserve derived dispatch.
				if (call->is_external)
					continue;
				if (is_class) {
					bool belongs_in_meta =
					    call->ty->kind == CLASS_METHOD;
					if (belongs_in_meta != in_meta)
						continue;
				} else if (in_meta) {
					continue;
				}

				fprintf(active, "\t");
				auto method =
				    dynamic_cast<Method*>(call);
				if (method && method->is_static)
					fprintf(active, "static ");
				else if (method &&
					 (is_interface ||
					  method->virtual_kind ==
					      Method::VirtualKind::Virtual ||
					  method->virtual_kind ==
					      Method::VirtualKind::Abstract ||
					  method->virtual_kind ==
					      Method::VirtualKind::Dynamic))
					fprintf(active, "virtual ");
				emit_callable_signature(
				    call, Position::Declaration, "");
				if (method) {
					if (method->virtual_kind ==
					    Method::VirtualKind::Override)
						fprintf(active, " override");
					if (method->is_final)
						fprintf(active, " final");
					if (is_interface)
						fprintf(active, " = 0");
					else if (method->virtual_kind ==
						 Method::VirtualKind::Abstract) {
						// Native FPC permits constructing a class which still
						// has abstract methods. Its VMT entry calls
						// AbstractError only if dispatch reaches that slot.
						// The System runtime-error hook lets SysUtils turn 211
						// into EAbstractError without making generated class
						// declarations depend on SysUtils.
						fprintf(active,
							" {\n"
							"\t\t::u_system::m_runtime_error(211);\n"
							"\t}\n");
						continue;
					}
				}
				fprintf(active, ";\n");
			}
		}
	}
	if (rec)
		emit_record_variant_decl(rec->variant, 1, 0);
	fprintf(active, "}");
}

void Emitter::emit_record_variant_decl(
    VariantPart* variant, unsigned indent,
    unsigned depth) {
	if (!variant)
		return;
	auto emit_indent = [&]() {
		for (unsigned i = 0; i < indent; ++i)
			fputc('\t', active);
	};
	if (variant->has_selector) {
		emit_indent();
		emit_type_ref(variant->selector_type);
		fprintf(active, " %s;\n",
			variant->selector_cxx_name.c_str());
	}
	if (variant->arms.empty())
		return;
	for (size_t arm_index = 0;
	     arm_index < variant->arms.size(); ++arm_index) {
		emit_indent();
		fprintf(active,
			"struct %s {\n",
			variant_arm_type_name(
			    depth, arm_index)
			    .c_str());
		for (const auto& field :
		     variant->arms[arm_index].fields) {
			for (unsigned i = 0; i < indent + 1; ++i)
				fputc('\t', active);
			emit_type_ref(field.ty);
			fprintf(active, " %s;\n",
				field.slot->cxx_name.c_str());
		}
		emit_record_variant_decl(
		    variant->arms[arm_index].variant,
		    indent + 1, depth + 1);
		emit_indent();
		fprintf(active, "};\n");
	}
	emit_indent();
	fprintf(active, "union m_variant_type {\n");
	for (size_t arm_index = 0;
	     arm_index < variant->arms.size(); ++arm_index) {
		for (unsigned i = 0; i < indent + 1; ++i)
			fputc('\t', active);
		fprintf(active,
			"%s m_arm_%zu;\n",
			variant_arm_type_name(
			    depth, arm_index)
			    .c_str(),
			arm_index);
	}
	emit_indent();
	fprintf(active, "} m_variant;\n");
}

void Emitter::emit_packed_record_decl(std::string cxx_name, PackedRecordType* p) {
	if (!active)
		return;
	if (cxx_name.empty())
		unhandled_type("anonymous packed records are not implemented", p);
	auto layout = packed_record_layout(p);
	if (!layout)
		unhandled_type("packed record layout is not known", p);

	fprintf(active, "struct %s {\n", cxx_name.c_str());
	for (size_t i = 0; i < layout->fields.size(); ++i) {
		const auto& field = layout->fields[i];
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
		// Offsets come from the shared compiler layout. They cannot be
		// reconstructed from the preceding emitted field once variants are
		// present: the next arm restarts at the same union offset, and a
		// nested variant restarts again inside its containing arm.
		fprintf(active,
			"\tenum : std::size_t { m_field_%zu_offset = %llu };\n",
			i, (unsigned long long)field.offset);
	}
	// Same local-class restriction as the field offsets above.
	fprintf(active,
		"\tenum : std::size_t { m_storage_size = %llu };\n",
		(unsigned long long)layout->type.size);
	fprintf(active, "\nprivate:\n");
	fprintf(active, "\tstd::array<std::byte, m_storage_size> m_storage{};\n");
	fprintf(active, "\npublic:\n");
	fprintf(active, "\tstd::byte* m_data() noexcept { return m_storage.data(); }\n");
	fprintf(active, "\tconst std::byte* m_data() const noexcept { return m_storage.data(); }\n");
	for (size_t i = 0; i < layout->fields.size(); ++i) {
		const auto& field = layout->fields[i];
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
	for (const auto& item :
	     p->children->value_declarations()) {
		Node* value = item.second.value;
		if (auto slot =
			dynamic_cast<StorageSlot*>(
			    value);
		    slot &&
		    slot->kind ==
			StorageSlot::Kind::StaticMember) {
			emit_static_member_declaration(slot);
			continue;
		}
		std::vector<Callable*> callables;
		if (auto callable =
			dynamic_cast<Callable*>(value))
			callables.push_back(callable);
		else if (auto overloads =
			     dynamic_cast<OverloadSet*>(
				 value))
			callables = overloads->members;
		for (Callable* callable : callables) {
			auto method =
			    dynamic_cast<Method*>(callable);
			if (!method || !method->is_static ||
			    method->ty->kind != ROUTINE)
				unhandled_node(
				    "packed record contains a non-static method",
				    callable);
			if (callable->is_external)
				continue;
			fprintf(active, "\tstatic ");
			emit_callable_signature(
			    callable, Position::Declaration, "");
			fprintf(active, ";\n");
		}
	}
	fprintf(active, "}");
}

void Emitter::emit_class_forward_declaration(
    std::string cxx_name) {
	if (!active)
		return;
	fprintf(active, "struct %s;\n",
		cxx_name.c_str());
}

void Emitter::emit_type_definition(std::string cxx_name, Type* ty) {
	if (!active)
		return;
	emit_type_dependencies(ty, true);
	if (dynamic_cast<RoutineType*>(ty)) {
		fprintf(active, "using %s = ", cxx_name.c_str());
		emit_type_ref(ty);
		fprintf(active, ";\n");
		return;
	}
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
		if (dynamic_cast<ClassType*>(ty)) {
			fprintf(active,
				"inline %s* %s::m_meta::m_allocate() {\n",
				cxx_name.c_str(), cxx_name.c_str());
			fprintf(active,
				"\treturn ::u_system::m_allocate_object<%s>();\n",
				cxx_name.c_str());
			fprintf(active, "}\n");
		}
		if (auto record = dynamic_cast<RecordType*>(ty)) {
			if (record->has_managed_lifetime()) {
				fprintf(active,
					"inline void m_pascal_initialize(%s& value) noexcept {\n",
					cxx_name.c_str());
				fprintf(active,
					"\tusing ::u_system::m_pascal_initialize;\n");
				for (const auto& field : record->fields)
					if (field.ty &&
					    field.ty->has_managed_lifetime())
						fprintf(active,
							"\tm_pascal_initialize(value.%s);\n",
							field.slot->cxx_name.c_str());
				fprintf(active, "}\n");

				fprintf(active,
					"inline void m_pascal_finalize(%s& value) noexcept {\n",
					cxx_name.c_str());
				fprintf(active,
					"\tusing ::u_system::m_pascal_finalize;\n");
				for (auto field = record->fields.rbegin();
				     field != record->fields.rend(); ++field)
					if (field->ty &&
					    field->ty->has_managed_lifetime())
						fprintf(active,
							"\tm_pascal_finalize(value.%s);\n",
							field->slot->cxx_name.c_str());
				fprintf(active, "}\n");
			}
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
			auto add_offset = [](const std::string& a,
					     const std::string& b) {
				return a == "0"
					   ? b
					   : a + " + " + b;
			};
			std::function<void(
			    VariantPart*, const std::string&,
			    const std::string&, unsigned)>
			    emit_variant_assertions;
			emit_variant_assertions =
			    [&](VariantPart* variant,
				const std::string& context_type,
				const std::string& context_offset,
				unsigned depth) {
				    if (!variant)
					    return;
				    if (variant->has_selector) {
					    const std::string offset =
						add_offset(
						    context_offset,
						    "offsetof(" + context_type +
							", " +
							variant->selector_cxx_name +
							")");
					    const std::string type =
						"decltype(" + context_type +
						"::" +
						variant->selector_cxx_name +
						")";
					    emit_field_assertions(
						variant->selector_slot,
						offset.c_str(), type.c_str());
				    }
				    if (variant->arms.empty())
					    return;
				    const std::string union_offset =
					add_offset(
					    context_offset,
					    "offsetof(" + context_type +
						", m_variant)");
				    for (size_t arm_index = 0;
					 arm_index < variant->arms.size();
					 ++arm_index) {
					    const auto& arm =
						variant->arms[arm_index];
					    const std::string arm_type =
						context_type + "::" +
						variant_arm_type_name(
						    depth, arm_index);
					    for (const auto& field :
						 arm.fields) {
						    const std::string offset =
							add_offset(
							    union_offset,
							    "offsetof(" +
								arm_type + ", " +
								field.slot->cxx_name +
								")");
						    const std::string type =
							"decltype(" +
							arm_type + "::" +
							field.slot->cxx_name +
							")";
						    emit_field_assertions(
							field.slot,
							offset.c_str(),
							type.c_str());
					    }
					    emit_variant_assertions(
						arm.variant, arm_type,
						union_offset, depth + 1);
				    }
			    };
			emit_variant_assertions(
			    record->variant, cxx_name, "0", 0);
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

void Emitter::emit_type_alias(
    std::string cxx_name, Type* aliased_type) {
	if (!active)
		return;
	emit_type_dependencies(aliased_type);
	std::string target =
	    named_type_local_cxx_name(aliased_type);
	if (target.empty())
		unhandled_type(
		    "named type alias target has no C++ name",
		    aliased_type);
	if (dynamic_cast<SubrangeType*>(
		aliased_type))
		// A local Pascal subrange declaration still uses the generated
		// canonical carrier directly, so its source-facing C++ alias may have
		// no emitted use. Keep -Werror from rejecting that valid declaration.
		fprintf(active,
			"using %s [[maybe_unused]] = %s;\n",
			cxx_name.c_str(),
			type_cxx_name(
			    aliased_type,
			    target)
			    .c_str());
	else
		fprintf(active, "using %s = %s;\n",
			cxx_name.c_str(),
			type_cxx_name(
			    aliased_type,
			    target)
			    .c_str());
}

void Emitter::emit_routine_reference(
    RoutineRef* reference) {
	if (!reference || !reference->resolved)
		unhandled_node(
		    "unresolved routine reference reached emission",
		    reference);
	if (auto procedure =
		dynamic_cast<Procedure*>(reference->resolved)) {
		if (reference->receiver)
			unhandled_node(
			    "standalone routine reference has a receiver",
			    reference);
		if (reference->code_only) {
			fprintf(active,
				"::u_system::m_function_to_code_pointer(static_cast<");
			emit_type_ref(procedure->ty->return_type);
			fprintf(active, " (*)");
			emit_formal_parameters(
			    procedure->ty, false,
			    nullptr);
			fprintf(active, ">(&%s))",
				node_cxx_name(
				    procedure,
				    callable_cxx_name(procedure))
				    .c_str());
		} else {
			fprintf(active, "&%s",
				node_cxx_name(
				    procedure,
				    callable_cxx_name(procedure))
				    .c_str());
		}
		return;
	}

	auto method =
	    dynamic_cast<Method*>(reference->resolved);
	if (!method)
		unhandled_node(
		    "routine reference resolved to neither procedure nor method",
		    reference);
	if (method->is_static) {
		if (reference->receiver ||
		    method->ty->kind != ROUTINE)
			unhandled_node(
			    "static method routine reference retained a receiver",
			    reference);
		std::string owner =
		    owner_cxx_reference_name(
			method->owner_class);
		if (owner.empty())
			unhandled_type(
			    "static method routine reference owner has no C++ name",
			    method->owner_class);
		if (reference->code_only) {
			fprintf(active,
				"::u_system::m_function_to_code_pointer(static_cast<");
			emit_type_ref(method->ty->return_type);
			fprintf(active, " (*)");
			emit_formal_parameters(
			    method->ty, false,
			    nullptr);
			fprintf(active, ">(&%s::%s))",
				owner.c_str(),
				callable_cxx_name(
				    method)
				    .c_str());
		} else {
			fprintf(active, "&%s::%s",
				owner.c_str(),
				callable_cxx_name(
				    method)
				    .c_str());
		}
		return;
	}
	if (!reference->receiver ||
	    (method->ty->kind != METHOD &&
	     method->ty->kind != CLASS_METHOD))
		unhandled_node(
		    "method routine reference is not receiver-bearing",
		    reference);
	std::string owner =
	    owner_cxx_reference_name(
		method->owner_class);
	if (owner.empty())
		unhandled_type(
		    "method routine reference owner has no C++ name",
		    method->owner_class);
	if (method->ty->kind == CLASS_METHOD)
		owner += "::m_meta";

	if (method->builtin_desc &&
	    method->builtin_desc->call_convention ==
		BuiltinCallConvention::ReceiverFirst) {
		// A receiver-first RTL method has no C++ pointer-to-member. Bind the
		// same external function and receiver into the ordinary two-word
		// Pascal method value; its adapter preserves the method ABI seen by
		// callers of `procedure of object`.
		fprintf(active,
			"::u_system::m_bind_receiver_function<static_cast<");
		emit_type_ref(method->ty->return_type);
		fprintf(active, " (*)(%s*", owner.c_str());
		for (const Parameter& formal :
		     method->ty->formals) {
			fprintf(active, ", ");
			emit_formal_parameter(formal, false);
		}
		fprintf(active, ")>(&%.*s)>(",
			(int)method->builtin_desc->cxx_name.size(),
			method->builtin_desc->cxx_name.data());
		if (reference->receiver->ty &&
		    reference->receiver->ty->is_reference_type()) {
			emit_expression(reference->receiver);
		} else {
			fprintf(active, "std::addressof(");
			emit_expression(reference->receiver);
			fprintf(active, ")");
		}
		fprintf(active, ")");
		if (reference->code_only)
			fprintf(active, ".p_code");
		return;
	}

	fprintf(active, "::u_system::m_bind_method<static_cast<");
	emit_type_ref(method->ty->return_type);
	fprintf(active, " (%s::*)", owner.c_str());
	emit_formal_parameters(
	    method->ty, false, nullptr);
	fprintf(active, ">(&%s::%s)>(",
		owner.c_str(), callable_cxx_name(method).c_str());
	if (method->ty->kind ==
	    CLASS_METHOD) {
		if (auto classref =
			dynamic_cast<ClassRefType*>(
			    reference->receiver->ty)) {
			auto target =
			    dynamic_cast<ClassType*>(
				classref->target);
			if (!target ||
			    target->cxx_name.empty())
				unhandled_type(
				    "class-method routine-reference target",
				    classref->target);
			fprintf(active,
				"static_cast<%s::m_meta*>(",
				type_cxx_name(
				    target,
				    target->cxx_name)
				    .c_str());
			emit_expression(
			    reference->receiver);
			fprintf(active, ")");
		} else if (dynamic_cast<ClassType*>(
			       reference->receiver->ty)) {
			emit_expression(
			    reference->receiver);
			fprintf(active, "->m_classref()");
		} else {
			unhandled_type(
			    "class-method routine-reference receiver",
			    reference->receiver->ty);
		}
	} else if (reference->receiver->ty &&
		   reference->receiver->ty->is_reference_type()) {
		emit_expression(reference->receiver);
	} else {
		fprintf(active, "std::addressof(");
		emit_expression(reference->receiver);
		fprintf(active, ")");
	}
	fprintf(active, ")");
	if (reference->code_only)
		fprintf(active, ".p_code");
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
	if (auto dereference =
		dynamic_cast<Dereference*>(expr);
	    dereference &&
	    dereference->ty == unknown_type()) {
		if (auto address =
			dynamic_cast<AddrOf*>(
			    dereference->a)) {
			// Pascal `(@place)^` is the original place. This matters for
			// omitted-type formals: their C++ carrier is already a storage
			// view, so taking the address of that carrier would point at
			// compiler bookkeeping rather than the caller's bytes.
			emit_storage_ref(address->a);
			return;
		}
		fprintf(active,
			"::u_system::tpcc_dereference_storage(");
		emit_expression(dereference->a);
		fprintf(active, ")");
		return;
	}
	if (auto property = dynamic_cast<PropertyAccess*>(expr)) {
		if (auto builtin =
			dynamic_cast<Builtin*>(
			    property->property
				->write_accessor)) {
			const bool unchecked =
			    builtin->desc &&
			    builtin->desc->cxx_name ==
				"::u_system::m_unchecked_index";
			fprintf(
			    active,
			    unchecked
				? "::u_system::m_unchecked_storage_ref("
				: "::u_system::tpcc_make_storage_ref(");
			emit_expression(property->receiver);
			for (Node* index : property->indexes) {
				fprintf(active, ", ");
				emit_expression(index);
			}
			fprintf(active, ")");
			return;
		}
	}
	fprintf(active, "::u_system::tpcc_make_storage_ref(");
	emit_writable_expression(expr);
	fprintf(active, ")");
}

void Emitter::emit_const_storage_ref(Node* expr) {
	if (auto dereference =
		dynamic_cast<Dereference*>(expr);
	    dereference &&
	    dereference->ty == unknown_type()) {
		if (auto address =
			dynamic_cast<AddrOf*>(
			    dereference->a)) {
			emit_const_storage_ref(address->a);
			return;
		}
		fprintf(active,
			"::u_system::tpcc_make_const_storage_ref("
			"::u_system::tpcc_dereference_storage(");
		emit_expression(dereference->a);
		fprintf(active, "))");
		return;
	}
	if (auto property = dynamic_cast<PropertyAccess*>(expr)) {
		if (auto builtin =
			dynamic_cast<Builtin*>(
			    property->property
				->read_accessor)) {
			const bool unchecked =
			    builtin->desc &&
			    builtin->desc->cxx_name ==
				"::u_system::m_unchecked_index";
			fprintf(
			    active,
			    unchecked
				? "::u_system::m_unchecked_const_storage_ref("
				: "::u_system::tpcc_make_const_storage_ref(");
			emit_expression(property->receiver);
			for (Node* index : property->indexes) {
				fprintf(active, ", ");
				emit_expression(index);
			}
			fprintf(active, ")");
			return;
		}
	}
	fprintf(active, "::u_system::tpcc_make_const_storage_ref(");
	emit_expression(expr);
	fprintf(active, ")");
}

void Emitter::emit_call_arguments(
    RoutineType* call_ty,
    const std::vector<Node*>& args) {
	for (size_t i = 0; i < args.size(); i++) {
		if (i > 0)
			fprintf(active, ", ");
		Node* arg = args[i];
		// An omitted value formal is a C++ template-deduction position, not a
		// real tpcc_unknown_type parameter. Pinning a literal to that
		// placeholder would emit an invalid cast to void* and let the backend
		// representation contradict the generic Pascal call already selected.
		if (call_ty && i < call_ty->formals.size() &&
		    (call_ty->formals[i].mode ==
			 ParamMode::Value ||
		     call_ty->formals[i].mode ==
			 ParamMode::Const) &&
		    dynamic_cast<Integer*>(arg) &&
		    (call_ty->formals[i].ty !=
			 unknown_type() ||
		     (call_ty->formals[i].mode ==
			  ParamMode::Value &&
		      arg->ty &&
		      arg->ty != unknown_type()))) {
			Type* formal_ty =
			    call_ty->formals[i].ty !=
				    unknown_type()
				? call_ty->formals[i].ty
				: arg->ty;
			// Pascal has already selected the declaration. Pin a raw
			// integer literal to its value/const-parameter carrier so C++
			// cannot independently select another overload. In particular,
			// an unqualified custom operator call must not let ADL prefer an
			// RTL function template merely because the literal would otherwise
			// be emitted as uint64_t. A generic value formal likewise uses
			// the actual's already-contextualized Pascal type: unknown is the
			// declaration's quantified T, not permission for C++ to infer a
			// different carrier from literal spelling.
			fprintf(active, "static_cast<");
			emit_type_ref(formal_ty);
			fprintf(active, ">(");
			emit_expression(arg);
			fprintf(active, ")");
		} else if (
		    call_ty && i < call_ty->formals.size() &&
		    call_ty->formals[i].mode == ParamMode::Const &&
		    call_ty->formals[i].ty == unknown_type()) {
			emit_const_storage_ref(arg);
		} else if (
		    call_ty && i < call_ty->formals.size() &&
		    (call_ty->formals[i].mode == ParamMode::Var ||
		     call_ty->formals[i].mode == ParamMode::Out) &&
		    call_ty->formals[i].ty == unknown_type()) {
			emit_storage_ref(arg);
		} else if (
		    call_ty && i < call_ty->formals.size() &&
		    (call_ty->formals[i].mode == ParamMode::Var ||
		     call_ty->formals[i].mode == ParamMode::Out)) {
			emit_writable_expression(arg);
		} else {
			emit_expression(arg);
		}
	}
}

static const char* cxx_unary_operator(UnaryOperation* op) {
	if (dynamic_cast<Dereference*>(op))
		return "*";
	return nullptr;
}

void Emitter::emit_expression(Node* expr) {
	if (!active)
		return;
	if (dynamic_cast<BuiltinEnumeratorCurrent*>(
		expr)) {
		fprintf(active,
			"tpcc_for_enumerator.m_current()");
		return;
	}
	if (auto view =
		dynamic_cast<OpenArrayConstView*>(
		    expr)) {
		fprintf(active,
			"::u_system::m_openarray_const_view(");
		emit_expression(view->a);
		fprintf(active, ")");
		return;
	}
	if (auto view =
		dynamic_cast<OpenArrayMutableView*>(
		    expr)) {
		fprintf(active,
			"::u_system::m_openarray_mutable_view(");
		emit_writable_expression(view->a);
		fprintf(active, ")");
		return;
	}
	if (auto view =
		dynamic_cast<OpenArrayOutView*>(
		    expr)) {
		fprintf(active,
			"::u_system::m_openarray_out_view(");
		emit_writable_expression(view->a);
		fprintf(active, ")");
		return;
	}
	if (auto copy =
		dynamic_cast<OpenArrayValueCopy*>(
		    expr)) {
		fprintf(active,
			"::u_system::m_openarray_value_copy(");
		emit_expression(copy->a);
		fprintf(active, ")");
		return;
	}
	if (auto sequence =
		dynamic_cast<EvaluateThen*>(expr)) {
		fprintf(active, "(static_cast<void>(");
		emit_expression(sequence->a);
		fprintf(active, "), ");
		emit_expression(sequence->b);
		fprintf(active, ")");
		return;
	}
	if (auto class_reference =
		dynamic_cast<ClassRefValue*>(expr)) {
		if (!class_reference->target ||
		    class_reference->target->cxx_name.empty())
			unhandled_node(
			    "class-reference value has unnamed target",
			    class_reference);
		fprintf(active, "%s::p_classtype()",
			type_cxx_name(
			    class_reference->target,
			    class_reference->target->cxx_name)
			    .c_str());
		return;
	}
	if (dynamic_cast<TypeMemberQualifier*>(expr))
		unhandled_node(
		    "type member qualifier reached value emission",
		    expr);
	if (auto nil = dynamic_cast<NilLiteral*>(expr)) {
		auto routine =
		    dynamic_cast<RoutineType*>(nil->ty);
		if (routine && routine->kind == METHOD) {
			emit_type_ref(routine);
			fprintf(active, "{}");
		} else if (routine && routine->kind == ROUTINE) {
			fprintf(active, "static_cast<");
			emit_type_ref(routine);
			fprintf(active, ">(nullptr)");
		} else {
			fprintf(active, "nullptr");
		}
		return;
	}
	if (auto c = dynamic_cast<Integer*>(expr)) {
		if (dynamic_cast<SubrangeType*>(c->ty)) {
			// A contextual literal already has its selected Pascal subrange
			// Type*. Construct that exact carrier through the same ordinal
			// storage operation used by ordinary conversions; there is no
			// implicit C++ conversion on the wrapper which could reopen
			// overload selection.
			fprintf(active,
				"::u_system::m_ordinal_cast<");
			emit_type_ref(c->ty);
			fprintf(active, ">(");
			emit_integer_literal(
			    active, c->value,
			    c->negative);
			fprintf(active, ")");
			return;
		}
		Type* ordinal_type = c->ty;
		const bool wrapped_ordinal =
		    ordinal_type == char_type() ||
		    dynamic_cast<EnumType*>(
			ordinal_type);
		if (wrapped_ordinal) {
			fprintf(active, "static_cast<");
			emit_type_ref(c->ty);
			fprintf(active, ">(");
		}
		emit_integer_literal(active, c->value, c->negative);
		if (wrapped_ordinal)
			fprintf(active, ")");
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
			fprintf(active, "static_cast<::u_system::t_char>(static_cast<uint8_t>(%u))",
				static_cast<unsigned>(static_cast<unsigned char>(s->value[0])));
			return;
		}
		auto shortstring =
		    dynamic_cast<ShortStringType*>(s->ty);
		if (!shortstring)
			unhandled_node(
			    "non-Char string literal has non-ShortString type",
			    s);
		fprintf(active, "::u_system::tpcc_shortstring_from_c<%u>(",
			static_cast<unsigned>(shortstring->capacity));
		fputc('"', active);
		for (unsigned char ch : s->value)
			fprintf(active, "\\%03o", static_cast<unsigned>(ch));
		fputc('"', active);
		fprintf(active, ", %zu)", s->value.size());
		return;
	}
	if (auto a = dynamic_cast<ArrayLiteral*>(expr)) {
		Type* item_type = nullptr;
		if (auto dynamic =
			dynamic_cast<DynamicArrayType*>(
			    a->ty)) {
			item_type = dynamic->item_type;
			emit_type_ref(dynamic);
			fprintf(active, "::m_from_values({");
		} else if (auto open =
			       dynamic_cast<OpenArrayType*>(
				   a->ty)) {
			item_type = open->item_type;
			fprintf(active,
				"::u_system::m_openarray_values<");
			emit_type_ref(item_type);
			fprintf(active, ">({");
		} else {
			unhandled_node(
			    "array literal has non-dynamic/open type",
			    a);
		}
		for (size_t i = 0;
		     i < a->elements.size(); ++i) {
			if (i)
				fprintf(active, ", ");
			emit_expression(a->elements[i]);
		}
		fprintf(active, "})");
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
	if (auto record = dynamic_cast<RecordLiteral*>(expr)) {
		Type* record_type = record->ty;
		const bool packed =
		    dynamic_cast<PackedRecordType*>(record_type);
		if (!packed &&
		    !dynamic_cast<RecordType*>(record_type))
			unhandled_node(
			    "record literal has non-record type", record);

		if (record->fields.empty()) {
			emit_type_ref(record_type);
			fprintf(active, "{}");
			return;
		}

		// Pass the recursively-emitted field expressions as lambda
		// arguments. A captureless lambda is valid both at namespace scope
		// and inside a routine, while still allowing a local field
		// expression to be evaluated at the call site.
		fprintf(active, "[](");
		for (size_t i = 0; i < record->fields.size(); ++i) {
			if (i)
				fprintf(active, ", ");
			emit_type_ref(record->fields[i].slot->ty);
			fprintf(active, " tpcc_field_%zu", i);
		}
		fprintf(active, ") { ");
		emit_type_ref(record_type);
		fprintf(active, " tpcc_record{}; ");
		for (size_t i = 0; i < record->fields.size(); ++i) {
			StorageSlot* slot = record->fields[i].slot;
			if (packed) {
				fprintf(active,
					"tpcc_record.m_set_%s(tpcc_field_%zu); ",
					slot->cxx_name.c_str(), i);
			} else {
				fprintf(active, "tpcc_record.");
				if (auto path =
					record_variant_path(
					    record_type, slot))
					fprintf(active, "%s",
						path->c_str());
				fprintf(active, "%s = tpcc_field_%zu; ",
					slot->cxx_name.c_str(), i);
			}
		}
		fprintf(active, "return tpcc_record; }(");
		for (size_t i = 0; i < record->fields.size(); ++i) {
			if (i)
				fprintf(active, ", ");
			emit_expression(record->fields[i].value);
		}
		fprintf(active, ")");
		return;
	}
	if (auto set = dynamic_cast<SetLiteral*>(expr)) {
		auto set_type = dynamic_cast<FixedSetType*>(set->ty);
		if (!set_type || set_type->item_type == unknown_type())
			unhandled_node("set literal has no contextual item type", set);
		fprintf(active, "::u_system::tpcc_make_set<");
		emit_type_ref(set_type->item_type);
		fprintf(active, ">({");
		for (size_t i = 0; i < set->items.size(); ++i) {
			if (i)
				fprintf(active, ", ");
			const SetLiteral::Item& item = set->items[i];
			if (item.upper) {
				fprintf(active, "::u_system::tpcc_set_range(");
				emit_expression(item.lower);
				fprintf(active, ", ");
				emit_expression(item.upper);
				fprintf(active, ")");
			} else {
				fprintf(active, "::u_system::tpcc_set_single(");
				emit_expression(item.lower);
				fprintf(active, ")");
			}
		}
		fprintf(active, "})");
		return;
	}
	if (auto constant =
		dynamic_cast<ConstantDecl*>(expr)) {
		emit_expression(constant->initializer);
		return;
	}
	if (auto s = dynamic_cast<StorageSlot*>(expr)) {
		if (s->kind ==
		    StorageSlot::Kind::StaticMember) {
			fprintf(active, "%s::%s",
				owner_cxx_reference_name(
				    s->owner_type)
				    .c_str(),
				s->cxx_name.c_str());
			if (s->initializer)
				fprintf(active, "()");
			return;
		}
		fprintf(active, "%s",
			node_cxx_name(s, s->cxx_name).c_str());
		return;
	}
	if (auto e = dynamic_cast<EnumMemberRef*>(expr)) {
		fprintf(active, "%s",
			node_cxx_name(e, e->cxx_name).c_str());
		return;
	}
	if (auto b = dynamic_cast<Builtin*>(expr)) {
		fprintf(active, "%.*s", (int)b->desc->cxx_name.size(), b->desc->cxx_name.data());
		return;
	}
	if (auto reference =
		dynamic_cast<RoutineRef*>(expr)) {
		emit_routine_reference(reference);
		return;
	}
	if (auto c = dynamic_cast<Callable*>(expr)) {
		fprintf(active, "%s",
			node_cxx_name(c, c->cxx_name).c_str());
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
		if (dynamic_cast<UnitRef*>(m->a)) {
			// A unit is a static declaration environment, not an object
			// receiver. The selected declaration already carries its owning
			// unit, so the ordinary owned-name emitter produces
			// `::u_unit::member`.
			emit_expression(m->b);
			return;
		}
		if (auto constant =
			dynamic_cast<ConstantDecl*>(m->b)) {
			emit_expression(constant);
			return;
		}
		if (auto slot = dynamic_cast<StorageSlot*>(m->b);
		    slot &&
		    slot->kind == StorageSlot::Kind::StaticMember) {
			emit_expression(slot);
			return;
		}
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
				fprintf(active, "%s", path->c_str());
			}
		}
		emit_expression(m->b);
		return;
	}
	if (auto o = dynamic_cast<ShortCircuitOperation*>(expr)) {
		// C++ &&/|| supply the required left-to-right short circuit, but their
		// result type is C++ bool. The CST result is Pascal Boolean, whose enum
		// carrier deliberately does not accept an implicit bool conversion.
		// Convert only the final result so the right operand remains lazy.
		fprintf(active,
			"::u_system::tpcc_bool_to_boolean(((");
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
		fprintf(active, ")))");
		return;
	}
	if (auto equal = dynamic_cast<RoutineEqual*>(expr)) {
		fprintf(active, "::u_system::m_equal(");
		emit_expression(equal->a);
		fprintf(active, ", ");
		emit_expression(equal->b);
		fprintf(active, ")");
		return;
	}
	if (auto ix = dynamic_cast<Index*>(expr)) {
		fprintf(active, "::u_system::p_index(");
		emit_expression(ix->a);
		fprintf(active, ", ");
		emit_expression(ix->b);
		fprintf(active, ")");
		return;
	}
	if (auto allocation =
		dynamic_cast<NewValue*>(expr)) {
		if (!allocation->allocated_type)
			unhandled_node(
			    "typed New without allocated type",
			    allocation);
		if (!allocation->initializer) {
			fprintf(active,
				"::u_system::m_new_value<");
			emit_type_ref(
			    allocation->allocated_type);
			fprintf(active, ">()");
			return;
		}
		Method* initializer =
		    allocation->initializer;
		if (!initializer->owner_class ||
		    initializer->ty->kind != CONSTRUCTOR)
			unhandled_node(
			    "invalid old-object initializer",
			    allocation);
		std::string owner =
		    owner_cxx_reference_name(
			initializer->owner_class);
		fprintf(active,
			"::u_system::m_new_object<");
		emit_type_ref(
		    allocation->allocated_type);
		fprintf(active, ", static_cast<");
		emit_type_ref(
		    initializer->ty->return_type);
		fprintf(active, " (%s::*)",
			owner.c_str());
		emit_formal_parameters(
		    initializer->ty, false,
		    nullptr);
		fprintf(active, ">(&%s::%s)>(",
			owner.c_str(),
			callable_cxx_name(initializer).c_str());
		emit_call_arguments(
		    initializer->ty,
		    allocation->args);
		fprintf(active, ")");
		return;
	}
	if (auto disposal =
		dynamic_cast<DisposeValue*>(expr)) {
		if (!disposal->pointer)
			unhandled_node(
			    "Dispose without pointer",
			    disposal);
		if (!disposal->finalizer) {
			fprintf(active,
				"::u_system::m_dispose_value(");
			emit_expression(
			    disposal->pointer);
			fprintf(active, ")");
			return;
		}
		Method* finalizer =
		    disposal->finalizer;
		if (!finalizer->owner_class ||
		    finalizer->ty->kind != DESTRUCTOR)
			unhandled_node(
			    "invalid old-object finalizer",
			    disposal);
		std::string owner =
		    owner_cxx_reference_name(
			finalizer->owner_class);
		fprintf(active,
			"::u_system::m_dispose_object<"
			"static_cast<");
		emit_type_ref(
		    finalizer->ty->return_type);
		fprintf(active, " (%s::*)",
			owner.c_str());
		emit_formal_parameters(
		    finalizer->ty, false,
		    nullptr);
		fprintf(active, ">(&%s::%s)>(",
			owner.c_str(),
			callable_cxx_name(finalizer).c_str());
		emit_expression(
		    disposal->pointer);
		fprintf(active, ")");
		return;
	}
	if (auto construct =
		dynamic_cast<Construct*>(expr)) {
		auto result_type =
		    dynamic_cast<ClassType*>(construct->ty);
		Method* initializer = construct->initializer;
		if (!result_type || !initializer ||
		    !initializer->owner_class)
			unhandled_node(
			    "invalid construction expression",
			    construct);
		std::string result_cxx_name =
		    type_cxx_name(
			result_type,
			result_type->cxx_name);
		std::string owner =
		    owner_cxx_reference_name(
			initializer->owner_class);
		fprintf(active,
			"::u_system::m_construct<%s, static_cast<",
			result_cxx_name.c_str());
		emit_type_ref(
		    initializer->ty->return_type);
		fprintf(active, " (%s::*)", owner.c_str());
		emit_formal_parameters(
		    initializer->ty, false,
		    nullptr);
		fprintf(active, ">(&%s::%s)>(",
			owner.c_str(),
			callable_cxx_name(initializer).c_str());
		fprintf(active, "static_cast<%s::m_meta*>(",
			result_cxx_name.c_str());
		emit_expression(
		    construct->class_reference);
		fprintf(active, ")");
		if (!construct->args.empty())
			fprintf(active, ", ");
		emit_call_arguments(
		    initializer->ty,
		    construct->args);
		fprintf(active, ")");
		return;
	}
	if (auto pc = dynamic_cast<ProcCall*>(expr)) {
		auto callable =
		    dynamic_cast<Callable*>(pc->callee);
		if (pc->lowering_builtin_desc) {
			if (pc->receiver || !callable)
				unhandled_node(
				    "call-site builtin lowering is not a standalone callable",
				    pc);
			// Ordinary Pascal lookup and argument conversion have already
			// selected `callable`. Only its compiler-owned implementation is
			// different at this caller-directive site, so emit the retained
			// descriptor with exactly the selected declaration's converted
			// arguments.
			fprintf(
			    active, "%.*s(",
			    static_cast<int>(
				pc->lowering_builtin_desc
				    ->cxx_name.size()),
			    pc->lowering_builtin_desc
				->cxx_name.data());
			emit_call_arguments(
			    callable->ty, pc->args);
			fprintf(active, ")");
			return;
		}
		if (auto method =
			dynamic_cast<Method*>(pc->callee);
		    method && method->is_static) {
			if (pc->receiver ||
			    method->ty->kind != ROUTINE)
				unhandled_node(
				    "static method call retained a receiver",
				    pc);
			std::string owner =
			    owner_cxx_reference_name(
				method->owner_class);
			if (owner.empty())
				unhandled_type(
				    "static method owner has no C++ name",
				    method->owner_class);
			fprintf(active, "%s::%s(",
				owner.c_str(),
				callable_cxx_name(method).c_str());
			emit_call_arguments(
			    method->ty, pc->args);
			fprintf(active, ")");
			return;
		}
		if (pc->receiver && callable &&
		    callable->builtin_desc &&
		    callable->builtin_desc->call_convention ==
			BuiltinCallConvention::ReceiverFirst) {
			// Do not enter a C++ member function to implement this Pascal
			// method. In particular, `nil.Free` must reach the nil-safe RTL
			// operation without first forming `nil->p_free()`. Passing the
			// receiver as a function argument also evaluates it exactly once.
			fprintf(active, "%.*s(",
				(int)callable->builtin_desc->cxx_name.size(),
				callable->builtin_desc->cxx_name.data());
			emit_expression(pc->receiver);
			if (!pc->args.empty())
				fprintf(active, ", ");
			emit_call_arguments(
			    callable->ty, pc->args);
			fprintf(active, ")");
			return;
		}
		const bool initializer_application =
		    pc->receiver && callable &&
		    callable->ty->kind == CONSTRUCTOR;
		if (initializer_application)
			fprintf(active,
				"::u_system::m_invoke_initializer"
				"([&]() { ");
		if (pc->receiver) {
			auto receiver = pc->receiver;
			bool done = false;
			if (auto method =
				dynamic_cast<Method*>(pc->callee)) {
				if (method->ty->kind == CLASS_METHOD) {
					if (auto classref =
						dynamic_cast<ClassRefType*>(
						    receiver->ty)) {
						auto target =
						    dynamic_cast<ClassType*>(
							classref->target);
						if (!target ||
						    target->cxx_name.empty())
							unhandled_type(
							    "class-method class-reference target",
							    classref->target);
						fprintf(active,
							"static_cast<%s::m_meta*>(",
							type_cxx_name(
							    target,
							    target->cxx_name)
							    .c_str());
						emit_expression(receiver);
						fprintf(active, ")->");
					} else if (dynamic_cast<ClassType*>(
						       receiver->ty)) {
						emit_expression(receiver);
						fprintf(active,
							"->m_classref()->");
					} else {
						unhandled_type(
						    "class-method receiver",
						    receiver->ty);
					}
					done = true;
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
			fprintf(active, "%s(",
				node_cxx_name(
				    c, callable_cxx_name(c))
				    .c_str());
		} else {
			emit_expression(pc->callee);
			fprintf(active, "(");
		}
		RoutineType* call_ty = nullptr;
		if (callable)
			call_ty = callable->ty;
		else
			call_ty =
			    dynamic_cast<RoutineType*>(
				pc->callee->ty);
		emit_call_arguments(call_ty, pc->args);
		if (callable &&
		    callable->is_conversion_operator()) {
			if (!pc->args.empty())
				fprintf(active, ", ");
			fprintf(active,
				"::u_system::m_conversion_target<");
			emit_type_ref(
			    callable->ty->return_type);
			fprintf(active, ">{}");
		}
		fprintf(active, ")");
		if (initializer_application)
			fprintf(active, "; })");
		return;
	}
	if (auto bound =
		dynamic_cast<ValueBound*>(expr)) {
		fprintf(active,
			bound->kind == TypeBoundKind::Low
			    ? "::u_system::p_low("
			    : "::u_system::p_high(");
		emit_expression(bound->a);
		fprintf(active, ")");
		return;
	}
	if (auto tb = dynamic_cast<TypeBound*>(expr)) {
		if (auto range =
			dynamic_cast<SubrangeType*>(
			    tb->operand_type)) {
			// Subranges erase to their base C++ carrier, so p_low<T>() and
			// p_high<T>() would describe the carrier rather than the Pascal
			// destination. Emit the declaration's actual constant bounds.
			emit_expression(
			    tb->kind ==
				    TypeBoundKind::Low
				? range->lower_bound
				: range->upper_bound);
			return;
		}
		if (auto enum_type =
			dynamic_cast<EnumType*>(
			    tb->operand_type)) {
			const auto* member =
			    tb->kind == TypeBoundKind::Low
				? enum_type->min_member()
				: enum_type->max_member();
			if (!member)
				unhandled_node(
				    "low/high of empty enum type", tb);
			fprintf(active, "%s",
				type_cxx_name(
				    enum_type,
				    member->cxx_name)
				    .c_str());
			return;
		}
		fprintf(active, tb->kind == TypeBoundKind::Low ? "::u_system::p_low<" : "::u_system::p_high<");
		emit_type_ref(tb->operand_type);
		fprintf(active, ">()");
		return;
	}
	if (auto size = dynamic_cast<SizeOf*>(expr)) {
		fprintf(active, "static_cast<::u_system::t_sizeint>(sizeof(");
		emit_type_ref(size->operand_type);
		fprintf(active, "))");
		return;
	}
	if (auto ca = dynamic_cast<Cast*>(expr)) {
		auto real_type =
		    [](Type* type) {
			    return type == single_type() ||
				   type == double_type() ||
				   type == extended_type();
		    };
		auto ordinal_type =
		    [](Type* type) {
			    while (auto range =
				       dynamic_cast<SubrangeType*>(
					   type))
				    type =
					range->base_type;
			    OrdinalBounds bounds;
			    return type == char_type() ||
				   dynamic_cast<EnumType*>(
				       type) ||
				   integer_bounds(
				       type, &bounds);
		    };
		if (dynamic_cast<RangeCheckedCast*>(
			ca)) {
			if (ca->a &&
			    real_type(ca->a->ty) &&
			    real_type(ca->ty)) {
				fprintf(active,
					"::u_system::m_range_checked_real_cast<");
				emit_type_ref(ca->ty);
				fprintf(active, ">(");
				emit_expression(ca->a);
				fprintf(active, ")");
				return;
			}
			TypeBound lower(
			    TypeBoundKind::Low,
			    ca->ty);
			TypeBound upper(
			    TypeBoundKind::High,
			    ca->ty);
			fprintf(active,
				"::u_system::m_range_checked_ordinal_cast<");
			emit_type_ref(ca->ty);
			fprintf(active, ">(");
			emit_expression(ca->a);
			fprintf(active, ", ");
			emit_expression(&lower);
			fprintf(active, ", ");
			emit_expression(&upper);
			fprintf(active, ")");
			return;
		}
		if (ca->a &&
		    real_type(ca->a->ty) &&
		    real_type(ca->ty)) {
			// Explicit real casts and {$R-} implicit narrowing are unchecked
			// Pascal conversions, but direct C++ floating narrowing is
			// undefined when a finite source exceeds the target range. The RTL
			// operation supplies the defined signed-infinity result.
			fprintf(active,
				"::u_system::m_real_cast<");
			emit_type_ref(ca->ty);
			fprintf(active, ">(");
			emit_expression(ca->a);
			fprintf(active, ")");
			return;
		}
		if (ca->a &&
		    ordinal_type(ca->a->ty) &&
		    ordinal_type(ca->ty)) {
			// Both explicit ordinal casts and {$R-} implicit conversions
			// are representation operations. The RTL path gives them
			// defined modulo/bit behavior instead of relying on C++'s
			// implementation-defined out-of-range signed conversions.
			fprintf(active,
				"::u_system::m_ordinal_cast<");
			emit_type_ref(ca->ty);
			fprintf(active, ">(");
			emit_expression(ca->a);
			fprintf(active, ")");
			return;
		}
		auto source_set =
		    dynamic_cast<FixedSetType*>(
			ca->a ? ca->a->ty : nullptr);
		auto target_set =
		    dynamic_cast<FixedSetType*>(
			ca->ty);
		if (source_set && target_set) {
			fprintf(active,
				"::u_system::m_set_cast<");
			emit_type_ref(
			    target_set->item_type);
			fprintf(active, ">(");
			emit_expression(ca->a);
			fprintf(active, ")");
			return;
		}
		auto source_shortstring =
		    dynamic_cast<ShortStringType*>(
			ca->a ? ca->a->ty : nullptr);
		auto target_shortstring =
		    dynamic_cast<ShortStringType*>(ca->ty);
		if (source_shortstring && target_shortstring) {
			fprintf(active,
				"::u_system::tpcc_shortstring_cast<%u>(",
				static_cast<unsigned>(
				    target_shortstring->capacity));
			emit_expression(ca->a);
			fprintf(active, ")");
			return;
		}
		const bool source_ansistring =
		    ca->a && ca->a->ty == ansistring_type();
		auto target_pointer =
		    dynamic_cast<PointerType*>(ca->ty);
		const bool target_pointer_integer =
		    ca->ty == ptrint_type() ||
		    ca->ty == ptruint_type();
		if (source_ansistring &&
		    (target_pointer ||
		     target_pointer_integer)) {
			if (target_pointer &&
			    target_pointer->is_untyped()) {
				fprintf(active, "(");
				emit_expression(ca->a);
				fprintf(active, ").m_pointer()");
			} else {
				fprintf(active,
					target_pointer_integer
					    ? "reinterpret_cast<"
					    : "static_cast<");
				emit_type_ref(ca->ty);
				fprintf(active, ">((");
				emit_expression(ca->a);
				fprintf(active, ").m_pointer())");
			}
			return;
		}
		auto source_pointer =
		    dynamic_cast<PointerType*>(
			ca->a ? ca->a->ty : nullptr);
		const bool source_object_reference =
		    ca->a &&
		    (dynamic_cast<ClassType*>(
			 ca->a->ty) ||
		     dynamic_cast<InterfaceType*>(
			 ca->a->ty));
		const bool target_object_reference =
		    dynamic_cast<ClassType*>(
			ca->ty) ||
		    dynamic_cast<InterfaceType*>(
			ca->ty);
		OrdinalBounds source_integer_bounds;
		Type* source_integer_type =
		    ca->a ? ca->a->ty : nullptr;
		const bool source_integer_wrapper =
		    dynamic_cast<SubrangeType*>(
			source_integer_type) != nullptr;
		while (auto range =
			   dynamic_cast<SubrangeType*>(
			       source_integer_type))
			source_integer_type =
			    range->base_type;
		const bool source_integer =
		    ca->a &&
		    (source_integer_type ==
			 &untyped_integer_type() ||
		     integer_bounds(
			 source_integer_type,
			 &source_integer_bounds));
		if ((source_pointer &&
		     (target_pointer ||
		      target_pointer_integer ||
		      target_object_reference)) ||
		    (source_object_reference &&
		     (target_pointer ||
		      target_pointer_integer)) ||
		    (source_integer &&
		     target_pointer)) {
			// Pascal explicit casts expose the pointer representation.
			// C++ static_cast cannot express integer/pointer crossings,
			// arbitrary typed-pointer reinterpretation, or recovery of a
			// class/interface reference from raw Pointer. This emits the C++
			// representation operation only: Pascal code inherits C++'s
			// provenance, lifetime, alignment, and aliasing preconditions, and
			// TPCC deliberately does not synthesize weaker runtime checks.
			fprintf(active, "reinterpret_cast<");
			emit_type_ref(ca->ty);
			fprintf(active, ">(");
			if (source_integer_wrapper &&
			    target_pointer) {
				// Subranges have a nominal wrapper carrier rather than a
				// C++ integral type. Expose its ordinal value through PtrUInt
				// for this one direct integer -> pointer lowering; this is not
				// a Pascal conversion edge visible to overload selection.
				fprintf(active,
					"::u_system::m_ordinal_cast<::u_system::t_ptruint>(");
				emit_expression(ca->a);
				fprintf(active, ")");
			} else {
				emit_expression(ca->a);
			}
			fprintf(active, ")");
			return;
		}
		auto source_routine = dynamic_cast<RoutineType*>(
		    ca->a ? ca->a->ty : nullptr);
		auto target_routine =
		    dynamic_cast<RoutineType*>(ca->ty);
		if (source_routine && target_routine) {
			// The parser has limited this explicit operation to one
			// unchanged routine-value representation with exact result,
			// arity, and modes; only by-value data-pointer formal types may
			// differ. The RTL helper makes the GNOME/GObject-style ABI
			// dependency explicit instead of pretending static_cast is a
			// portable C++ function-pointer conversion.
			fprintf(active,
				"::u_system::m_explicit_routine_cast<");
			emit_function_type(target_routine);
			fprintf(active, ">(");
			emit_expression(ca->a);
			fprintf(active, ")");
			return;
		}
		if (source_routine &&
		    source_routine->kind == METHOD &&
		    ca->ty == tmethod_type()) {
			fprintf(active, "::u_system::m_method_to_tmethod(");
			emit_expression(ca->a);
			fprintf(active, ")");
			return;
		}
		if (ca->a && ca->a->ty == tmethod_type() &&
		    target_routine &&
		    target_routine->kind == METHOD) {
			fprintf(active, "::u_system::m_tmethod_to_method<");
			emit_function_type(target_routine);
			fprintf(active, ">(");
			emit_expression(ca->a);
			fprintf(active, ")");
			return;
		}
		auto source_classref =
		    dynamic_cast<ClassRefType*>(
			ca->a ? ca->a->ty : nullptr);
		auto target_classref =
		    dynamic_cast<ClassRefType*>(ca->ty);
		if (source_classref && target_classref) {
			auto source_class =
			    dynamic_cast<ClassType*>(
				source_classref->target);
			auto target_class =
			    dynamic_cast<ClassType*>(
				target_classref->target);
			if (!source_class || !target_class ||
			    source_class->cxx_name.empty() ||
			    target_class->cxx_name.empty())
				unhandled_type(
				    "class-reference conversion target",
				    ca->ty);
			fprintf(active,
				"static_cast<::u_system::m_classref<%s>*>(static_cast<%s::m_meta*>(static_cast<%s::m_meta*>(",
				type_cxx_name(
				    target_class,
				    target_class->cxx_name)
				    .c_str(),
				type_cxx_name(
				    target_class,
				    target_class->cxx_name)
				    .c_str(),
				type_cxx_name(
				    source_class,
				    source_class->cxx_name)
				    .c_str());
			emit_expression(ca->a);
			fprintf(active, ")))");
			return;
		}
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
		bool numeric =
		    co->target_type == single_type() ||
		    co->target_type == double_type() ||
		    co->target_type == extended_type();
		bool real_source =
		    co->a &&
		    (co->a->ty == single_type() ||
		     co->a->ty == double_type() ||
		     co->a->ty == extended_type());
		if (numeric && real_source) {
			// `as` is explicit and therefore ignores {$R}, but it shares the
			// defined unchecked real conversion instead of exposing C++
			// out-of-range conversion behavior.
			fprintf(active,
				"::u_system::m_real_cast<");
			emit_type_ref(co->target_type);
			fprintf(active, ">(");
			emit_expression(co->a);
			fprintf(active, ")");
			return;
		}
		fprintf(active, numeric
				    ? "static_cast<"
				    : "dynamic_cast<");
		emit_type_ref(co->target_type);
		fprintf(active, ">(");
		emit_expression(co->a);
		fprintf(active, ")");
		return;
	}
	if (auto co = dynamic_cast<CoerceCheck*>(expr)) {
		fprintf(active, "::u_system::tpcc_bool_to_boolean(dynamic_cast<");
		emit_type_ref(co->target_type);
		fprintf(active, ">(");
		emit_expression(co->a);
		fprintf(active, ") != nullptr)");
		return;
	}
	if (auto address =
		dynamic_cast<AddrOf*>(expr)) {
		// Pascal has no const-qualified pointer type: @Place has semantic type
		// ^T even when Place is currently viewed through a const formal. Spell
		// that cv removal explicitly instead of relying on an ill-formed C++
		// reinterpret_cast later. Writing through the result is valid only
		// when the underlying C++ object is not actually const.
		fprintf(active, "const_cast<");
		emit_type_ref(address->ty);
		fprintf(active, ">(std::addressof(");
		emit_writable_expression(address->a);
		fprintf(active, "))");
		return;
	}
	if (auto u = dynamic_cast<UnaryOperation*>(expr)) {
		if (const char* op = cxx_unary_operator(u)) {
			if (auto dereference =
				dynamic_cast<Dereference*>(u);
			    dereference &&
			    dereference->ty == unknown_type())
				unhandled_node(
				    "untyped pointer dereference used as a value",
				    dereference);
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
			owner_cxx_reference_name(
			    m->owner_class)
			    .c_str(),
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
		fprintf(active, "%s",
			node_cxx_name(e, e->cxx_name).c_str());
		return;
	}
	unhandled_node("emit_template_value_arg", expr);
}

void Emitter::emit_type_ref(Type* ty) {
	if (!active)
		return;
	if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		if (s->cxx_name.empty())
			unhandled_type(
			    "subrange has no generated C++ carrier name",
			    s);
		fprintf(active, "%s",
			type_cxx_name(
			    s, s->cxx_name)
			    .c_str());
		return;
	}
	if (auto it = dynamic_cast<IntrinsicType*>(ty)) {
		fprintf(active, "%.*s", (int)it->cxx_name.size(), it->cxx_name.data());
		return;
	}
	if (auto shortstring = dynamic_cast<ShortStringType*>(ty)) {
		fprintf(active, "::u_system::t_shortstring<%u>",
			static_cast<unsigned>(shortstring->capacity));
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
			fprintf(active, "%s",
				type_cxx_name(r, r->cxx_name).c_str());
		return;
	}
	if (auto r = dynamic_cast<PackedRecordType*>(ty)) {
		if (r->cxx_name.empty())
			unhandled_type("anonymous packed record type reference", ty);
		fprintf(active, "%s",
			type_cxx_name(r, r->cxx_name).c_str());
		return;
	}
	if (auto c = dynamic_cast<ClassType*>(ty)) {
		if (c->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(active, "%s",
				type_cxx_name(c, c->cxx_name).c_str());
		fprintf(active, "*");
		return;
	}
	if (auto r = dynamic_cast<ClassRefType*>(ty)) {
		ty = r->target;
		if (auto c = dynamic_cast<ClassType*>(ty)) {
			if (c->cxx_name.empty())
				unhandled_type(
				    "class-reference target has no emitted type binding",
				    c);
			fprintf(active,
				"::u_system::m_classref<%s>",
				type_cxx_name(
				    c, c->cxx_name)
				    .c_str());
		} else {
			unhandled_type("emit_type_ref", ty);
		}
		fprintf(active, "*");
		return;
	}
	if (auto c = dynamic_cast<InterfaceType*>(ty)) {
		if (c->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(active, "%s",
				type_cxx_name(c, c->cxx_name).c_str());
		fprintf(active, "*");
		return;
	}
	if (auto o = dynamic_cast<ObjectType*>(ty)) {
		if (o->cxx_name.empty())
			emit_aggregate_decl("", ty);
		else
			fprintf(active, "%s",
				type_cxx_name(o, o->cxx_name).c_str());
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
			fprintf(active, "%s",
				type_cxx_name(e, e->cxx_name).c_str());
		return;
	}
	if (auto p = dynamic_cast<PointerType*>(ty)) {
		if (p->is_untyped()) {
			if (p->cxx_name.empty())
				unhandled_type(
				    "untyped pointer has no C++ carrier",
				    p);
			fprintf(active, "%s",
				p->cxx_name.c_str());
		} else {
			emit_type_ref(p->item_type);
			fprintf(active, "*");
		}
		return;
	}
	if (auto f = dynamic_cast<TypedFileType*>(ty)) {
		fprintf(active, "::u_system::t_typedfile<");
		emit_type_ref(f->item_type);
		fprintf(active, ">");
		return;
	}
	if (auto a = dynamic_cast<DynamicArrayType*>(ty)) {
		fprintf(active, "::u_system::t_dynamicarray<");
		emit_type_ref(a->item_type);
		fprintf(active, ">");
		return;
	}
	if (auto a = dynamic_cast<OpenArrayType*>(ty)) {
		fprintf(active, "::u_system::t_openarray<");
		emit_type_ref(a->item_type);
		fprintf(active, ">");
		return;
	}
	if (auto rt = dynamic_cast<RoutineType*>(ty)) {
		if (rt->kind == METHOD)
			fprintf(active, "::u_system::m_method<");
		else if (rt->kind == ROUTINE)
			fprintf(active, "::u_system::m_proc<");
		else
			unhandled_type(
			    "declaration-only routine kind used as a routine value",
			    rt);
		emit_function_type(rt);
		fprintf(active, ">");
		return;
	}
	if (auto s = dynamic_cast<FixedSetType*>(ty)) {
		fprintf(active, "::u_system::t_set<");
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
