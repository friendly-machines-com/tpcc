#include "types.h"
#include "builtins.h"
#include <cassert>
#include <utility>

Type::Type(SourceLocation source_location) : source_location(std::move(source_location)) {}

IncompleteType::IncompleteType(SourceLocation source_location, std::string name)
    : Type(std::move(source_location)), name(std::move(name)), resolved(nullptr) {}

EnumType::EnumType(SourceLocation source_location) : Type(std::move(source_location)), cxx_name("") {
}

EnumType::EnumType(SourceLocation source_location, std::string p_cxx_name, std::string a, std::string b)
    : Type(std::move(source_location)), cxx_name(std::move(p_cxx_name)) {
	members.push_back(Member{
	    .pas_name = a,
	    .cxx_name = a,
	    .value = 0,
	});
	members.push_back(Member{
	    .pas_name = b,
	    .cxx_name = b,
	    .value = 1,
	});
}

FixedArrayType::FixedArrayType(SourceLocation source_location, Type* bounds, OrdinalRange range, Type* item_type)
    : Type(std::move(source_location)) {
	this->bounds = bounds;
	this->range = range;
	this->item_type = item_type;
}

FixedSetType::FixedSetType(SourceLocation source_location, Type* item_type)
    : Type(std::move(source_location)) {
	this->item_type = item_type;
}

PointerType::PointerType(SourceLocation source_location, Type* item_type)
    : Type(std::move(source_location)) {
	this->item_type = item_type;
}

RecordType::RecordType(SourceLocation source_location, Frame* children, bool packed)
    : Type(std::move(source_location)) {
	this->children = children;
	this->packed = packed;
}

ClassType::ClassType(SourceLocation source_location, Frame* children, std::vector<InterfaceType*> implemented_interfaces, ClassType* super)
    : Type(std::move(source_location)) {
	this->children = children;
	this->implemented_interfaces = std::move(implemented_interfaces);
	this->super = super;
}

InterfaceType::InterfaceType(SourceLocation source_location, Frame* children, std::vector<InterfaceType*> super_interfaces)
    : Type(std::move(source_location)) {
	this->children = children;
	this->super_interfaces = std::move(super_interfaces);
}

InterfaceType::InterfaceType(SourceLocation source_location, std::string cxx_name, Frame* children, std::vector<InterfaceType*> super_interfaces)
    : Type(std::move(source_location)) {
	this->cxx_name = std::move(cxx_name);
	this->children = children;
	this->super_interfaces = std::move(super_interfaces);
}

ObjectType::ObjectType(SourceLocation source_location, Frame* children, ObjectType* super)
    : Type(std::move(source_location)) {
	this->children = children;
	this->super = super;
}

ModuleType::ModuleType(SourceLocation source_location, Frame* interface_children, Frame* implementation_children)
    : Type(std::move(source_location)) {
	this->interface_children = interface_children;
	this->implementation_children = implementation_children;
}

UnitType::UnitType(SourceLocation source_location) : Type(std::move(source_location)) {}
UntypedIntegerType::UntypedIntegerType(SourceLocation source_location) : Type(std::move(source_location)) {}

RoutineType::RoutineType(SourceLocation source_location, std::vector<Parameter> formals, Type* return_type, RoutineKind kind)
    : Type(std::move(source_location)) {
	this->formals = std::move(formals);
	this->return_type = return_type;
	this->kind = kind;
}

// Integer widening rank; -1 for non-integer types.
static int integer_widening_rank(Type* ty) {
	auto it = dynamic_cast<IntrinsicType*>(ty);
	if (!it)
		return -1;
	if (!it->rank)
		return -1;
	return *(it->rank);
}

static bool is_real_intrinsic(Type* ty) {
	return ty == double_type();
}

Type* common_arith_type(Type* a, Type* b) {
	if (!a || !b)
		return nullptr;
	if (a == &untyped_integer_type() && b == &untyped_integer_type())
		return integer_type();
	if (a == b)
		return a;
	if (a == shortstring_type() && b == char_type()) {
		return shortstring_type();
	} else if (a == char_type() && b == shortstring_type()) {
		return shortstring_type();
	}
	if (a == &untyped_integer_type())
		return b;
	if (b == &untyped_integer_type())
		return a;
	int ra = integer_widening_rank(a), rb = integer_widening_rank(b);
	if (ra < 0 || rb < 0)
		return nullptr;
	return (ra >= rb) ? a : b;
}

// FIXME: add enums, sets; add class-to-interface via implemented_interfaces
int conversion_cost(Type* from, Type* to) {
	if (!from || !to)
		return -1;
	if (from == to)
		return 0;
	if (from == &untyped_integer_type())
		return 0; // literal adapts to any int
	// Subclass-to-superclass: implicit, cost = depth (1 per inheritance step).
	// Identity handled by `from == to` above.
	if (from->is_reference_type() && to->is_reference_type()) {
		int depth = 0;
		for (ClassType* cur = dynamic_cast<ClassType*>(from); cur; cur = cur->super, ++depth)
			if (cur->super == to)
				return depth + 1;
		depth = 0;
		for (ObjectType* cur = dynamic_cast<ObjectType*>(from); cur; cur = cur->super, ++depth)
			if (cur->super == to)
				return depth + 1;
		// METAclass references: class of Derived -> class of Base
		if (auto from_classref = dynamic_cast<ClassRefType*>(from)) {
			if (auto to_classref = dynamic_cast<ClassRefType*>(to)) {
				int depth = 0;
				// FIXME: Handle IncompleteType.
				for (ClassType* cur = dynamic_cast<ClassType*>(from_classref->target); cur; cur = cur->super, ++depth) {
					if (cur == to_classref->target)
						return depth;
				}
				return -1;
			}
		}
		return -1;
	}
	int rfrom = integer_widening_rank(from), rto = integer_widening_rank(to);
	if (rfrom >= 0 && rto >= 0 && rto >= rfrom)
		return 1;
	// Pascal permits integer-to-real assignment/conversion. This can be lossy:
	// large Int64/QWord values are not all exactly representable as double. Keep
	// the rule explicit here instead of pretending it is another integer widening.
	if (rfrom >= 0 && is_real_intrinsic(to))
		return 2;
	return -1;
}

// A dominates B iff A's cost is <= B's on every position AND strictly < on
// at least one. Different-length vectors don't compare (ambiguity later).
bool dominates(const std::vector<int>& a, const std::vector<int>& b) {
	if (a.size() != b.size())
		return false;
	bool strict = false;
	for (size_t i = 0; i < a.size(); i++) {
		if (a[i] > b[i])
			return false;
		if (a[i] < b[i])
			strict = true;
	}
	return strict;
}

#include "cst.h"
#include "diagnostic.h"
#include "frame.h"
#include <cstdio>

static std::string diagnostic_string_literal(const std::string& text) {
	std::string r = "'";
	for (char ch : text) {
		if (ch == '\'')
			r.push_back('\'');
		r.push_back(ch);
	}
	r.push_back('\'');
	return r;
}
// Aggregate frames are indexed for member names by add_frame_edge(), but
// indexing frames is deliberately name-evidence-only. Aggregate type bodies
// print member type refs, so the owning aggregate Type explicitly contributes
// the value-entry Type* edges through this helper. Do not move this discovery
// into ErrorLetContext::index_frame().
static void add_frame_value_type_edges(ErrorLetContext* ctx, const Frame* frame) {
	if (!frame)
		return;
	for (const auto& item : frame->values_local()) {
		ctx->add_type_edge(item.second.ty);
	}
}

void Type::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "truncated: yes";
}

const char* IncompleteType::diagnostic_kind() const { return "incomplete"; }
void IncompleteType::collect_diagnostic_edges(ErrorLetContext* ctx) const { ctx->add_type_edge(resolved); }
void IncompleteType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "name: " << diagnostic_string_literal(name);
	if (resolved) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "resolved: " << ctx->known_type_ref(resolved);
	}
}

const char* FixedArrayType::diagnostic_kind() const { return "array"; }
void FixedArrayType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(bounds);
	ctx->add_value_edge(range.lower_bound);
	ctx->add_value_edge(range.upper_bound);
	ctx->add_type_edge(item_type);
}
void FixedArrayType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "bounds: " << ctx->known_type_ref(bounds);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "lower_bound: " << ctx->known_value_ref(range.lower_bound);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "upper_bound: " << ctx->known_value_ref(range.upper_bound);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "length: " << range.length;
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "item: " << ctx->known_type_ref(item_type);
}
void FixedArrayType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "bounds: ...";
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "item: ...";
}

const char* FixedSetType::diagnostic_kind() const { return "set"; }
void FixedSetType::collect_diagnostic_edges(ErrorLetContext* ctx) const { ctx->add_type_edge(item_type); }
void FixedSetType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "item: " << ctx->known_type_ref(item_type);
}

const char* EnumType::diagnostic_kind() const { return "enum"; }
void EnumType::collect_diagnostic_edges(ErrorLetContext*) const {}
void EnumType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "";
	for (const auto& m : members) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << (m.pas_name.empty() ? "<member>" : m.pas_name) << " = " << m.value;
	}
	out << "\n";
	ctx->indent(out, indent);
	out << "end";
}

const char* RecordType::diagnostic_kind() const { return "record"; }
void RecordType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
	ctx->add_type_edge(selector_type);
	for (const auto& arm : arms)
		for (const auto& field : arm.fields)
			ctx->add_type_edge(field.ty);
}
void RecordType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	if (packed) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "packed: yes";
	}
	out << "\n";
	ctx->print_frame_members(out, children, indent + 1);
	if (has_selector) {
		ctx->indent(out, indent + 1);
		out << "case " << selector_name << ": " << ctx->known_type_ref(selector_type) << " of\n";
		// Variant labels are parsed for layout/selection but not retained in
		// VariantArm yet; the diagnostic can still show the important structural
		// information: which overlapping fields exist in each arm and their types.
		// Use numbered arms until VariantArm stores source labels.
		for (size_t i = 0; i < arms.size(); ++i) {
			ctx->indent(out, indent + 2);
			out << "arm " << (i + 1) << ":\n";
			for (const auto& field : arms[i].fields) {
				ctx->indent(out, indent + 3);
				if (!field.pas_name.empty())
					out << field.pas_name;
				else
					out << "<field>";
				out << ": " << ctx->known_type_ref(field.ty) << ";\n";
			}
		}
	}
	ctx->indent(out, indent);
	out << "end";
}
void RecordType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const { out << " ... end"; }

const char* InterfaceType::diagnostic_kind() const { return "interface"; }
void InterfaceType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
	for (auto* i : super_interfaces)
		ctx->add_type_edge(i);
}
void InterfaceType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	if (!super_interfaces.empty()) {
		ctx->indent(out, indent + 1);
		out << "inherits:";
		for (auto* i : super_interfaces)
			out << " " << ctx->known_type_ref(i);
		out << "\n";
	}
	ctx->print_frame_members(out, children, indent + 1);
	ctx->indent(out, indent);
	out << "end";
}
void InterfaceType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const { out << " ... end"; }

const char* ClassType::diagnostic_kind() const { return "class"; }
void ClassType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(super);
	for (auto* i : implemented_interfaces)
		ctx->add_type_edge(i);
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
}
void ClassType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	if (super) {
		ctx->indent(out, indent + 1);
		out << "super: " << ctx->known_type_ref(super) << "\n";
	}
	if (!implemented_interfaces.empty()) {
		ctx->indent(out, indent + 1);
		out << "implements:";
		for (auto* i : implemented_interfaces)
			out << " " << ctx->known_type_ref(i);
		out << "\n";
	}
	ctx->print_frame_members(out, children, indent + 1);
	ctx->indent(out, indent);
	out << "end";
}
void ClassType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const { out << " ... end"; }

const char* ClassRefType::diagnostic_kind() const { return "classref"; }
void ClassRefType::collect_diagnostic_edges(ErrorLetContext* ctx) const { ctx->add_type_edge(target); }
void ClassRefType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "target: " << ctx->known_type_ref(target);
}
void ClassRefType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "target: ...";
}

const char* ObjectType::diagnostic_kind() const { return "object"; }
void ObjectType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(super);
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
}
void ObjectType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	if (super) {
		ctx->indent(out, indent + 1);
		out << "super: " << ctx->known_type_ref(super) << "\n";
	}
	ctx->print_frame_members(out, children, indent + 1);
	ctx->indent(out, indent);
	out << "end";
}
void ObjectType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const { out << " ... end"; }

const char* PointerType::diagnostic_kind() const { return "pointer"; }
void PointerType::collect_diagnostic_edges(ErrorLetContext* ctx) const { ctx->add_type_edge(item_type); }
void PointerType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "to: " << ctx->known_type_ref(item_type);
}
void PointerType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "to: ...";
}

const char* ModuleType::diagnostic_kind() const { return "module"; }
void ModuleType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(interface_children, DiagnosticFrameUse::ModuleMembers);
	ctx->add_frame_edge(implementation_children, DiagnosticFrameUse::ModuleMembers);
}
void ModuleType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "details: ...";
}
void ModuleType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "details: ...";
}

const char* UnitType::diagnostic_kind() const { return "unit"; }
void UnitType::collect_diagnostic_edges(ErrorLetContext*) const {}
void UnitType::print_diagnostic_definition(ErrorLetContext*, std::ostringstream&, unsigned) const {}

const char* UntypedIntegerType::diagnostic_kind() const { return "untyped_integer"; }
void UntypedIntegerType::collect_diagnostic_edges(ErrorLetContext*) const {}
void UntypedIntegerType::print_diagnostic_definition(ErrorLetContext*, std::ostringstream&, unsigned) const {}

static const char* param_mode_text(ParamMode mode) {
	switch (mode) {
	case ParamMode::Value:
		return "";
	case ParamMode::Var:
		return "var ";
	case ParamMode::Out:
		return "out ";
	case ParamMode::Const:
		return "const ";
	}
	return "";
}

const char* RoutineType::diagnostic_kind() const { return "routine"; }

void RoutineType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	for (const auto& p : formals) {
		ctx->add_type_edge(p.ty);
		ctx->add_value_edge(p.default_value);
	}
	ctx->add_type_edge(return_type);
}

void RoutineType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	if (kind == CONSTRUCTOR || kind == DESTRUCTOR || kind == CLASS_METHOD) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "kind: ";
		if (kind == CONSTRUCTOR)
			out << "constructor";
		else if (kind == DESTRUCTOR)
			out << "destructor";
		else
			out << "class_method";
	}
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "signature: (";
	for (size_t i = 0; i < formals.size(); i++) {
		if (i)
			out << "; ";
		const auto& p = formals[i];
		out << param_mode_text(p.mode) << p.pas_name << ": " << ctx->known_type_ref(p.ty);
		if (p.default_value)
			out << " = " << ctx->known_value_ref(p.default_value);
	}
	out << ")";
	if (return_type)
		out << ": " << ctx->known_type_ref(return_type);
}

void RoutineType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "signature: ...";
}

SubrangeType::SubrangeType(SourceLocation source_location, Type* base_type, Node* lower_bound, Node* upper_bound) : Type(std::move(source_location)) {
	this->base_type = base_type;
	this->lower_bound = lower_bound;
	this->upper_bound = upper_bound;
	// FIXME: assert(higher_bound >= lower_bound);
	assert(this->lower_bound->ty == base_type);
	assert(this->upper_bound->ty == base_type);
}

const char* SubrangeType::diagnostic_kind() const {
	return "subrange";
}

void SubrangeType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(base_type);
	ctx->add_value_edge(lower_bound);
	ctx->add_value_edge(upper_bound);
}

void SubrangeType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "lower_bound: " << ctx->known_value_ref(lower_bound);

	out << "\n";
	ctx->indent(out, indent + 1);
	out << "upper_bound: " << ctx->known_value_ref(upper_bound);

	out << "\n";
	ctx->indent(out, indent + 1);
	out << "base: " << ctx->known_type_ref(base_type);
}

void SubrangeType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "lower_bound: ...";
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "upper_bound: ...";
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "base: ...";
}
