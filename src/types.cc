#include "types.h"
#include "builtins.h"
#include "evaluator.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <set>
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

RecordType::RecordType(SourceLocation source_location, Frame* children)
    : Type(std::move(source_location)) {
	this->children = children;
}

PackedRecordType::PackedRecordType(SourceLocation source_location, Frame* children)
    : Type(std::move(source_location)) {
	this->children = children;
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

namespace {

bool checked_add_u64(uint64_t a, uint64_t b, uint64_t* result) {
	if (b > std::numeric_limits<uint64_t>::max() - a)
		return false;
	*result = a + b;
	return true;
}

bool checked_multiply_u64(uint64_t a, uint64_t b, uint64_t* result) {
	if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a)
		return false;
	*result = a * b;
	return true;
}

bool align_up_u64(uint64_t value, uint64_t alignment, uint64_t* result) {
	if (alignment == 0)
		return false;
	const uint64_t remainder = value % alignment;
	return remainder == 0
	    ? (*result = value, true)
	    : checked_add_u64(value, alignment - remainder, result);
}

std::optional<TypeLayout> type_layout_impl(
    Type* ty, std::set<Type*>& visiting);

struct SequentialLayout {
	uint64_t offset = 0;
	uint64_t alignment = 1;
	std::vector<AggregateFieldLayout> fields;
};

bool append_aligned_field(SequentialLayout& layout,
    StorageSlot* slot, Type* ty, std::set<Type*>& visiting) {
	auto field_layout = type_layout_impl(ty, visiting);
	if (!field_layout)
		return false;
	uint64_t offset;
	if (!align_up_u64(layout.offset, field_layout->alignment, &offset))
		return false;
	uint64_t end;
	if (!checked_add_u64(offset, field_layout->size, &end))
		return false;
	layout.fields.push_back(
	    AggregateFieldLayout{slot, ty, offset, field_layout->size});
	layout.offset = end;
	layout.alignment = std::max(layout.alignment, field_layout->alignment);
	return true;
}

std::optional<RecordLayout> record_layout_impl(
    RecordType* record, std::set<Type*>& visiting) {
	if (!visiting.insert(record).second)
		return std::nullopt;

	SequentialLayout fixed;
	for (const auto& field : record->fields) {
		if (!append_aligned_field(
		        fixed, field.slot, field.ty, visiting)) {
			visiting.erase(record);
			return std::nullopt;
		}
	}
	if (record->has_selector &&
	    !append_aligned_field(
	        fixed, record->selector_slot,
	        record->selector_type, visiting)) {
		visiting.erase(record);
		return std::nullopt;
	}

	if (!record->arms.empty()) {
		uint64_t union_size = 0;
		uint64_t union_alignment = 1;
		std::vector<std::vector<AggregateFieldLayout>> arm_fields;
		for (const auto& arm : record->arms) {
			SequentialLayout arm_layout;
			for (const auto& field : arm.fields) {
				if (!append_aligned_field(
				        arm_layout, field.slot,
				        field.ty, visiting)) {
					visiting.erase(record);
					return std::nullopt;
				}
			}
			uint64_t arm_size;
			if (!align_up_u64(
			        arm_layout.offset == 0 ? 1 : arm_layout.offset,
			        arm_layout.alignment, &arm_size)) {
				visiting.erase(record);
				return std::nullopt;
			}
			union_size = std::max(union_size, arm_size);
			union_alignment =
			    std::max(union_alignment, arm_layout.alignment);
			arm_fields.push_back(std::move(arm_layout.fields));
		}

		uint64_t union_offset;
		if (!align_up_u64(
		        fixed.offset, union_alignment, &union_offset) ||
		    !checked_add_u64(
		        union_offset, union_size, &fixed.offset)) {
			visiting.erase(record);
			return std::nullopt;
		}
		fixed.alignment =
		    std::max(fixed.alignment, union_alignment);
		for (auto& fields : arm_fields) {
			for (auto& field : fields) {
				field.offset += union_offset;
				fixed.fields.push_back(field);
			}
		}
	}

	uint64_t size;
	if (!align_up_u64(
	        fixed.offset == 0 ? 1 : fixed.offset,
	        fixed.alignment, &size)) {
		visiting.erase(record);
		return std::nullopt;
	}
	visiting.erase(record);
	return RecordLayout{
	    TypeLayout{size, fixed.alignment},
	    std::move(fixed.fields),
	};
}

std::optional<TypeLayout> type_layout_impl(
    Type* ty, std::set<Type*>& visiting) {
	while (auto incomplete = dynamic_cast<IncompleteType*>(ty)) {
		if (!incomplete->resolved)
			return std::nullopt;
		ty = incomplete->resolved;
	}
	if (auto intrinsic = dynamic_cast<IntrinsicType*>(ty))
		return intrinsic->layout;
	if (ty == boolean_type())
		return TypeLayout{1, 1};
	if (dynamic_cast<EnumType*>(ty))
		return TypeLayout{4, 4};
	if (auto subrange = dynamic_cast<SubrangeType*>(ty))
		return type_layout_impl(subrange->base_type, visiting);
	if (auto array = dynamic_cast<FixedArrayType*>(ty)) {
		auto item = type_layout_impl(array->item_type, visiting);
		if (!item)
			return std::nullopt;
		uint64_t size;
		if (!checked_multiply_u64(
		        item->size, array->range.length, &size))
			return std::nullopt;
		return TypeLayout{size, item->alignment};
	}
	if (dynamic_cast<FixedSetType*>(ty))
		return TypeLayout{24, 8};
	if (dynamic_cast<PointerType*>(ty) ||
	    dynamic_cast<ClassType*>(ty) ||
	    dynamic_cast<InterfaceType*>(ty) ||
	    dynamic_cast<ClassRefType*>(ty))
		return TypeLayout{8, 8};
	if (auto record = dynamic_cast<RecordType*>(ty)) {
		auto layout = record_layout_impl(record, visiting);
		return layout
		    ? std::optional<TypeLayout>{layout->type}
		    : std::nullopt;
	}
	if (auto packed = dynamic_cast<PackedRecordType*>(ty)) {
		if (!visiting.insert(packed).second)
			return std::nullopt;
		uint64_t size = 0;
		for (const auto& field : packed->fields) {
			auto field_layout =
			    type_layout_impl(field.ty, visiting);
			if (!field_layout ||
			    !checked_add_u64(
			        size, field_layout->size, &size)) {
				visiting.erase(packed);
				return std::nullopt;
			}
		}
		visiting.erase(packed);
		return TypeLayout{size == 0 ? 1 : size, 1};
	}
	if (auto routine = dynamic_cast<RoutineType*>(ty)) {
		if (routine->kind == METHOD)
			return TypeLayout{16, 8};
		return TypeLayout{8, 8};
	}
	return std::nullopt;
}

} // namespace

std::optional<TypeLayout> type_layout(Type* ty) {
	std::set<Type*> visiting;
	return type_layout_impl(ty, visiting);
}

std::optional<RecordLayout> record_layout(RecordType* record) {
	std::set<Type*> visiting;
	return record_layout_impl(record, visiting);
}

// Integer widening rank; -1 for non-integer types.
static int integer_widening_rank(Type* ty) {
	while (auto s = dynamic_cast<SubrangeType*>(ty))
		ty = s->base_type;
	auto it = dynamic_cast<IntrinsicType*>(ty);
	if (!it)
		return -1;
	if (!it->rank)
		return -1;
	return *(it->rank);
}

// Pascal real-family widening order. Keep this independent from the integer
// rank stored on IntrinsicType: those ranks describe ordinal overloads and
// bounds, while real widening has different semantics.
static int real_widening_rank(Type* ty) {
	if (ty == double_type())
		return 0;
	if (ty == extended_type())
		return 1;
	return -1;
}

static bool ordinal_bounds_contain_range(const OrdinalBounds& outer, const OrdinalBounds& inner) {
	if (inner.signed_type) {
		if (!outer.signed_type || outer.min_magnitude < inner.min_magnitude)
			return false;
	}
	return outer.max_positive >= inner.max_positive;
}

static bool integer_like_bounds(Type* ty, OrdinalBounds* out) {
	if (integer_bounds(ty, out))
		return true;
	if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		ConstEvalContext ctx;
		ConstEvalResult lower = s->lower_bound->const_eval(ctx);
		ConstEvalResult upper = s->upper_bound->const_eval(ctx);
		if (lower.kind != ConstEvalResult::Kind::Success || upper.kind != ConstEvalResult::Kind::Success)
			return false;
		auto lo = dynamic_cast<Integer*>(lower.node);
		auto hi = dynamic_cast<Integer*>(upper.node);
		if (!lo || !hi)
			return false;
		out->signed_type = lo->negative;
		out->min_magnitude = lo->negative ? lo->value : 0;
		out->max_positive = hi->negative ? 0 : hi->value;
		return true;
	}
	return false;
}

static uint64_t saturating_add(uint64_t a, uint64_t b) {
	if (a > UINT64_MAX - b)
		return UINT64_MAX;
	return a + b;
}

static uint64_t unsigned_abs_diff(uint64_t a, uint64_t b) {
	return a >= b ? a - b : b - a;
}

static uint64_t ordinal_lower_bound_distance(const OrdinalBounds& a, const OrdinalBounds& b) {
	if (a.signed_type && b.signed_type)
		return unsigned_abs_diff(a.min_magnitude, b.min_magnitude);
	if (a.signed_type)
		return a.min_magnitude;
	if (b.signed_type)
		return b.min_magnitude;
	return 0;
}

static int bit_width(uint64_t value) {
	int result = 0;
	while (value != 0) {
		++result;
		value >>= 1;
	}
	return result;
}

static int integer_conversion_cost(Type* from, Type* to) {
	int rfrom = integer_widening_rank(from), rto = integer_widening_rank(to);
	if (rfrom < 0 || rto < 0)
		return -1;

	OrdinalBounds from_bounds;
	OrdinalBounds to_bounds;
	if (!integer_like_bounds(from, &from_bounds) || !integer_like_bounds(to, &to_bounds))
		return -1;

	uint64_t distance = ordinal_lower_bound_distance(from_bounds, to_bounds);
	distance = saturating_add(distance, unsigned_abs_diff(from_bounds.max_positive, to_bounds.max_positive));

	int cost = 10 + bit_width(distance) * 2;
	if (from_bounds.signed_type != to_bounds.signed_type)
		++cost;
	if (!ordinal_bounds_contain_range(to_bounds, from_bounds))
		cost += 200;
	return cost;
}

static bool routine_signature_type_equal(
    Type* a, Type* b) {
	if (a == b)
		return true;
	return conversion_cost(a, b) == 0 &&
	       conversion_cost(b, a) == 0;
}

bool routine_types_compatible(
    const RoutineType* from, const RoutineType* to) {
	if (!from || !to || from->kind != to->kind)
		return false;
	if (from->kind != ROUTINE && from->kind != METHOD)
		return false;
	if (!routine_signature_type_equal(
	        from->return_type, to->return_type) ||
	    from->formals.size() != to->formals.size())
		return false;
	for (size_t i = 0; i < from->formals.size(); ++i) {
		const Parameter& a = from->formals[i];
		const Parameter& b = to->formals[i];
		if (a.mode != b.mode)
			return false;
		// Pointer and nested routine types can be independently parsed but
		// structurally identical. Require zero-cost compatibility both ways;
		// widening and other implicit conversions are not signature identity.
		if (!routine_signature_type_equal(a.ty, b.ty))
			return false;
	}
	return true;
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
	if (ra >= 0 && rb >= 0)
		return (ra >= rb) ? a : b;
	int rra = real_widening_rank(a), rrb = real_widening_rank(b);
	if (rra >= 0 && rrb >= 0)
		return (rra >= rrb) ? a : b;
	if (rra >= 0 && rb >= 0)
		return a;
	if (rrb >= 0 && ra >= 0)
		return b;
	return nullptr;
}

// FIXME: add enums, sets; add class-to-interface via implemented_interfaces
int conversion_cost(Type* from, Type* to) {
	if (!from || !to)
		return -1;
	if (from == to)
		return 0;
	if (auto from_routine = dynamic_cast<RoutineType*>(from)) {
		auto to_routine = dynamic_cast<RoutineType*>(to);
		return to_routine &&
		       routine_types_compatible(from_routine, to_routine)
		    ? 0
		    : -1;
	}
	if (from == &untyped_integer_type())
		return 0; // literal adapts to any int
	// Char and Byte remain nominally distinct (so exact overloads can
	// distinguish them), but Pascal permits ordinal conversion between their
	// identical unsigned eight-bit ranges.
	if ((from == char_type() && to == byte_type()) ||
	    (from == byte_type() && to == char_type()))
		return 20;
	// Set types are structural in their ordinal item type. Empty set literals
	// carry unknown_type() until context supplies the destination item type.
	if (auto from_set = dynamic_cast<FixedSetType*>(from)) {
		if (auto to_set = dynamic_cast<FixedSetType*>(to)) {
			if (from_set->item_type == unknown_type())
				return 0;
			return conversion_cost(from_set->item_type, to_set->item_type);
		}
	}
	// Pointer types are structural in Pascal. Independently-created `^T`
	// nodes with the same target are assignment-compatible.
	if (auto from_pointer = dynamic_cast<PointerType*>(from))
		if (auto to_pointer = dynamic_cast<PointerType*>(to))
			if (from_pointer->item_type == to_pointer->item_type)
				return 0;
	// Pascal's untyped Pointer is assignment-compatible with every typed
	// object pointer. Cast emission performs the corresponding C++ void*
	// conversion; no pointer representation is copied bytewise.
	if ((from == pointer_type() && dynamic_cast<PointerType*>(to)) ||
	    (dynamic_cast<PointerType*>(from) && to == pointer_type()))
		return 20;
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
	int rfrom = integer_widening_rank(from);
	int int_cost = integer_conversion_cost(from, to);
	if (int_cost >= 0)
		return int_cost;
	int real_from = real_widening_rank(from);
	int real_to = real_widening_rank(to);
	if (real_from >= 0 && real_to >= 0) {
		int cost = 2 + std::abs(real_to - real_from);
		if (real_to < real_from)
			cost += 200; // permitted narrowing, never preferred by overloads
		return cost;
	}
	// Pascal permits integer-to-real assignment/conversion. This can be lossy:
	// large Int64/QWord values are not all exactly representable. Any viable
	// integer overload must beat a real overload for integer operands, so keep
	// this in a cost band above even widening to Int64/QWord. Double still beats
	// Extended when both real overloads are otherwise candidates.
	if (rfrom >= 0 && real_to >= 0)
		return 500 + real_to;
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

const char* PackedRecordType::diagnostic_kind() const { return "packed record"; }
void PackedRecordType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
	for (const auto& field : fields)
		ctx->add_type_edge(field.ty);
}
void PackedRecordType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->print_frame_members(out, children, indent + 1);
	ctx->indent(out, indent);
	out << "end";
}
void PackedRecordType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const { out << " ... end"; }

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
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "kind: ";
	if (kind == CONSTRUCTOR)
		out << "constructor";
	else if (kind == DESTRUCTOR)
		out << "destructor";
	else if (kind == METHOD)
		out << "method";
	else if (kind == ROUTINE)
		out << "routine";
	else
		out << "class_method";
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
