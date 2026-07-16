#include "types.h"
#include "builtins.h"
#include "cst.h"
#include "evaluator.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <set>
#include <utility>

Type::Type(SourceLocation source_location) : source_location(std::move(source_location)) {}

std::optional<ValueConversion>
Type::value_conversion_from(const Type*) const {
	return std::nullopt;
}

bool Type::is_subtype_of(const Type* target) const {
	return this == target;
}

bool Type::same_cxx_carrier_as(
    const Type* other) const {
	const Type* a = this;
	const Type* b = other;
	while (auto range =
		   dynamic_cast<const SubrangeType*>(a))
		a = range->base_type;
	while (auto range =
		   dynamic_cast<const SubrangeType*>(b))
		b = range->base_type;
	return a && b &&
	       (a == b ||
		a->same_cxx_carrier_definition_as(b));
}

bool Type::same_cxx_carrier_definition_as(
    const Type*) const {
	// Records, packed records, objects, classes, interfaces, and enums are
	// nominal C++ declarations. Type* identity in the public wrapper is their
	// complete carrier relation.
	return false;
}

IncompleteType::IncompleteType(SourceLocation source_location, std::string name)
    : Type(std::move(source_location)), name(std::move(name)), resolved(nullptr) {}

EnumType::EnumType(SourceLocation source_location) : Type(std::move(source_location)), cxx_name("") {
}

EnumType::EnumType(
    SourceLocation source_location,
    std::string p_cxx_name, std::string a,
    std::string b, unsigned p_carrier_bits,
    bool p_carrier_signed)
    : Type(std::move(source_location)),
      cxx_name(std::move(p_cxx_name)),
      carrier_bits(p_carrier_bits),
      carrier_signed(p_carrier_signed) {
	members.push_back(Member{
	    .pas_name = a,
	    .cxx_name = cxx_name + "::p_" + a,
	    .value = 0,
	});
	members.push_back(Member{
	    .pas_name = b,
	    .cxx_name = cxx_name + "::p_" + b,
	    .value = 1,
	});
}

const EnumType::Member* EnumType::min_member() const {
	if (members.empty())
		return nullptr;
	return &*std::min_element(
	    members.begin(), members.end(),
	    [](const Member& a, const Member& b) {
		    return a.value < b.value;
	    });
}

const EnumType::Member* EnumType::max_member() const {
	if (members.empty())
		return nullptr;
	return &*std::max_element(
	    members.begin(), members.end(),
	    [](const Member& a, const Member& b) {
		    return a.value < b.value;
	    });
}

FixedArrayType::FixedArrayType(SourceLocation source_location, Type* bounds, OrdinalRange range, Type* item_type)
    : Type(std::move(source_location)) {
	this->bounds = bounds;
	this->range = range;
	this->item_type = item_type;
}

ShortStringType::ShortStringType(
    SourceLocation source_location, uint8_t capacity)
    : Type(std::move(source_location)), capacity(capacity) {
	assert(capacity != 0);
}

FixedSetType::FixedSetType(SourceLocation source_location, Type* item_type)
    : Type(std::move(source_location)) {
	this->item_type = item_type;
}

TypedFileType::TypedFileType(
    SourceLocation source_location, Type* item_type)
    : Type(std::move(source_location)), item_type(item_type) {
}

PointerType::PointerType(
    SourceLocation source_location, Type* item_type,
    std::string cxx_name)
    : Type(std::move(source_location)),
      item_type(item_type),
      cxx_name(std::move(cxx_name)) {}

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
	this->needs_vmt = super && super->needs_vmt;
}

ModuleType::ModuleType(
    SourceLocation source_location, Frame* children)
    : Type(std::move(source_location)) {
	this->children = children;
}

UnitType::UnitType(SourceLocation source_location) : Type(std::move(source_location)) {}
UntypedIntegerType::UntypedIntegerType(SourceLocation source_location) : Type(std::move(source_location)) {}

RoutineType::RoutineType(SourceLocation source_location, std::vector<Parameter> formals, Type* return_type, RoutineKind kind)
    : Type(std::move(source_location)) {
	this->formals = std::move(formals);
	this->return_type = return_type;
	this->kind = kind;
}

bool ShortStringType::
    same_cxx_carrier_definition_as(
	const Type* other) const {
	auto string =
	    dynamic_cast<const ShortStringType*>(other);
	return string &&
	       capacity == string->capacity;
}

bool FixedArrayType::
    same_cxx_carrier_definition_as(
	const Type* other) const {
	auto array =
	    dynamic_cast<const FixedArrayType*>(other);
	if (!array ||
	    range.length != array->range.length ||
	    range.lower_ordinal.negative !=
		array->range.lower_ordinal.negative ||
	    range.lower_ordinal.magnitude !=
		array->range.lower_ordinal.magnitude ||
	    !item_type->same_cxx_carrier_as(
		array->item_type))
		return false;
	Type* lower_type =
	    range.lower_bound
		? range.lower_bound->ty
		: nullptr;
	Type* other_lower_type =
	    array->range.lower_bound
		? array->range.lower_bound->ty
		: nullptr;
	return lower_type && other_lower_type &&
	       lower_type->same_cxx_carrier_as(
		   other_lower_type);
}

bool FixedSetType::
    same_cxx_carrier_definition_as(
	const Type* other) const {
	auto set =
	    dynamic_cast<const FixedSetType*>(other);
	return set &&
	       item_type->same_cxx_carrier_as(
		   set->item_type);
}

bool TypedFileType::
    same_cxx_carrier_definition_as(
	const Type* other) const {
	auto file =
	    dynamic_cast<const TypedFileType*>(other);
	return file &&
	       item_type->same_cxx_carrier_as(
		   file->item_type);
}

bool ClassRefType::
    same_cxx_carrier_definition_as(
	const Type* other) const {
	auto reference =
	    dynamic_cast<const ClassRefType*>(other);
	// m_classref<T> contains the nominal target as a template argument.
	return reference &&
	       target == reference->target;
}

bool PointerType::
    same_cxx_carrier_definition_as(
	const Type* other) const {
	auto pointer =
	    dynamic_cast<const PointerType*>(other);
	if (!pointer ||
	    is_untyped() != pointer->is_untyped())
		return false;
	if (is_untyped())
		return cxx_name == pointer->cxx_name;
	return item_type->same_cxx_carrier_as(
	    pointer->item_type);
}

bool RoutineType::
    same_cxx_carrier_definition_as(
	const Type* other) const {
	auto routine =
	    dynamic_cast<const RoutineType*>(other);
	return routine && kind == routine->kind &&
	       return_type->same_cxx_carrier_as(
		   routine->return_type) &&
	       same_cxx_parameter_list_as(routine);
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

bool append_aligned_variant(SequentialLayout& layout,
			    VariantPart* variant, std::set<Type*>& visiting) {
	if (!variant)
		return true;
	if (variant->has_selector &&
	    !append_aligned_field(
		layout, variant->selector_slot,
		variant->selector_type, visiting))
		return false;
	if (variant->arms.empty())
		return true;

	uint64_t union_size = 0;
	uint64_t union_alignment = 1;
	std::vector<std::vector<AggregateFieldLayout>> arm_fields;
	for (const auto& arm : variant->arms) {
		SequentialLayout arm_layout;
		for (const auto& field : arm.fields)
			if (!append_aligned_field(
				arm_layout, field.slot,
				field.ty, visiting))
				return false;
		if (!append_aligned_variant(
			arm_layout, arm.variant, visiting))
			return false;

		uint64_t arm_size;
		if (!align_up_u64(
			arm_layout.offset == 0 ? 1 : arm_layout.offset,
			arm_layout.alignment, &arm_size))
			return false;
		union_size = std::max(union_size, arm_size);
		union_alignment =
		    std::max(union_alignment, arm_layout.alignment);
		arm_fields.push_back(std::move(arm_layout.fields));
	}

	uint64_t union_offset;
	if (!align_up_u64(
		layout.offset, union_alignment, &union_offset) ||
	    !checked_add_u64(
		union_offset, union_size, &layout.offset))
		return false;
	layout.alignment =
	    std::max(layout.alignment, union_alignment);
	for (auto& fields : arm_fields)
		for (auto& field : fields) {
			if (!checked_add_u64(
				field.offset, union_offset,
				&field.offset))
				return false;
			layout.fields.push_back(field);
		}
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
	if (!append_aligned_variant(
		fixed, record->variant, visiting)) {
		visiting.erase(record);
		return std::nullopt;
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

struct PackedSequentialLayout {
	uint64_t offset = 0;
	std::vector<AggregateFieldLayout> fields;
};

bool append_packed_field(PackedSequentialLayout& layout,
			 StorageSlot* slot, Type* ty, std::set<Type*>& visiting) {
	auto field_layout = type_layout_impl(ty, visiting);
	if (!field_layout)
		return false;
	uint64_t end;
	if (!checked_add_u64(
		layout.offset, field_layout->size, &end))
		return false;
	layout.fields.push_back(
	    AggregateFieldLayout{
		slot, ty, layout.offset, field_layout->size});
	layout.offset = end;
	return true;
}

bool append_packed_variant(PackedSequentialLayout& layout,
			   VariantPart* variant, std::set<Type*>& visiting) {
	if (!variant)
		return true;
	if (variant->has_selector &&
	    !append_packed_field(
		layout, variant->selector_slot,
		variant->selector_type, visiting))
		return false;
	if (variant->arms.empty())
		return true;

	const uint64_t union_offset = layout.offset;
	uint64_t union_size = 0;
	std::vector<std::vector<AggregateFieldLayout>> arm_fields;
	for (const auto& arm : variant->arms) {
		PackedSequentialLayout arm_layout;
		for (const auto& field : arm.fields)
			if (!append_packed_field(
				arm_layout, field.slot,
				field.ty, visiting))
				return false;
		if (!append_packed_variant(
			arm_layout, arm.variant, visiting))
			return false;
		union_size = std::max(union_size, arm_layout.offset);
		arm_fields.push_back(std::move(arm_layout.fields));
	}
	if (!checked_add_u64(
		union_offset, union_size, &layout.offset))
		return false;
	for (auto& fields : arm_fields)
		for (auto& field : fields) {
			if (!checked_add_u64(
				field.offset, union_offset,
				&field.offset))
				return false;
			layout.fields.push_back(field);
		}
	return true;
}

std::optional<RecordLayout> packed_record_layout_impl(
    PackedRecordType* record, std::set<Type*>& visiting) {
	if (!visiting.insert(record).second)
		return std::nullopt;
	PackedSequentialLayout layout;
	for (const auto& field : record->fields)
		if (!append_packed_field(
			layout, field.slot, field.ty, visiting)) {
			visiting.erase(record);
			return std::nullopt;
		}
	if (!append_packed_variant(
		layout, record->variant, visiting)) {
		visiting.erase(record);
		return std::nullopt;
	}
	visiting.erase(record);
	return RecordLayout{
	    TypeLayout{layout.offset == 0 ? 1 : layout.offset, 1},
	    std::move(layout.fields),
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
	if (auto shortstring = dynamic_cast<ShortStringType*>(ty))
		return TypeLayout{
		    static_cast<uint64_t>(shortstring->capacity) + 1, 1};
	if (auto enumeration =
		dynamic_cast<EnumType*>(ty)) {
		uint64_t bytes =
		    enumeration->carrier_bits / 8;
		return enumeration->carrier_bits != 0 &&
			       enumeration->carrier_bits % 8 == 0
			   ? std::optional<TypeLayout>{
				 TypeLayout{bytes, bytes}}
			   : std::nullopt;
	}
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
	if (dynamic_cast<TypedFileType*>(ty))
		return TypeLayout{8, 8};
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
		auto layout =
		    packed_record_layout_impl(packed, visiting);
		return layout
			   ? std::optional<TypeLayout>{layout->type}
			   : std::nullopt;
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

std::optional<RecordLayout> packed_record_layout(
    PackedRecordType* record) {
	std::set<Type*> visiting;
	return packed_record_layout_impl(record, visiting);
}

// Integer widening rank; -1 for non-integer types.
static int integer_widening_rank(
    const Type* ty) {
	while (auto s =
		   dynamic_cast<const SubrangeType*>(ty))
		ty = s->base_type;
	auto it =
	    dynamic_cast<const IntrinsicType*>(ty);
	if (!it)
		return -1;
	if (!it->rank)
		return -1;
	return *(it->rank);
}

// Pascal real-family widening order. Keep this independent from the integer
// rank stored on IntrinsicType: those ranks describe ordinal overloads and
// bounds, while real widening has different semantics.
static int real_widening_rank(
    const Type* ty) {
	if (ty == single_type())
		return 0;
	if (ty == double_type())
		return 1;
	if (ty == extended_type())
		return 2;
	return -1;
}

static bool ordinal_bounds_contain_range(const OrdinalBounds& outer, const OrdinalBounds& inner) {
	if (inner.signed_type) {
		if (!outer.signed_type || outer.min_magnitude < inner.min_magnitude)
			return false;
	}
	return outer.max_positive >= inner.max_positive;
}

static bool integer_like_bounds(
    const Type* ty, OrdinalBounds* out) {
	if (integer_bounds(ty, out))
		return true;
	if (auto s =
		dynamic_cast<const SubrangeType*>(ty)) {
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

static int integer_conversion_cost(
    const Type* from, const Type* to) {
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

static ValueConversion direct_conversion(unsigned distance = 0) {
	return ValueConversion{
	    ValueConversionClass::Direct, distance};
}

static ValueConversion implicit_conversion(unsigned distance = 0) {
	return ValueConversion{
	    ValueConversionClass::Convert, distance};
}

std::optional<ValueConversion>
IntrinsicType::value_conversion_from(
    const Type* source_const) const {
	auto source = source_const;
	auto target = this;
	if (source == &untyped_integer_type()) {
		// The expression matcher checks the literal's actual magnitude. At the
		// type level it is a contextual integer value, not another nominal
		// integer definition.
		if (integer_widening_rank(target) >= 0)
			return direct_conversion();
		if (real_widening_rank(target) >= 0)
			return implicit_conversion(
			    500 + real_widening_rank(target));
	}

	if ((source == char_type() && target == byte_type()) ||
	    (source == byte_type() && target == char_type()))
		return implicit_conversion(20);

	int integer_cost =
	    integer_conversion_cost(source, target);
	if (integer_cost >= 0)
		return implicit_conversion(
		    static_cast<unsigned>(integer_cost));

	int source_real = real_widening_rank(source);
	int target_real = real_widening_rank(target);
	if (source_real >= 0 && target_real >= 0) {
		unsigned distance = static_cast<unsigned>(
		    std::abs(target_real - source_real));
		if (target_real < source_real)
			distance += 200;
		return implicit_conversion(distance);
	}
	if (integer_widening_rank(source) >= 0 &&
	    target_real >= 0)
		return implicit_conversion(
		    500 + static_cast<unsigned>(target_real));
	return std::nullopt;
}

std::optional<ValueConversion>
ShortStringType::value_conversion_from(
    const Type* source) const {
	auto string =
	    dynamic_cast<const ShortStringType*>(source);
	if (!string)
		return std::nullopt;
	unsigned distance = static_cast<unsigned>(
	    std::abs(static_cast<int>(capacity) -
		     static_cast<int>(string->capacity)));
	if (capacity == string->capacity)
		return direct_conversion();
	if (string->capacity > capacity)
		distance += 256;
	return implicit_conversion(distance);
}

std::optional<ValueConversion>
FixedSetType::value_conversion_from(
    const Type* source) const {
	auto set = dynamic_cast<const FixedSetType*>(source);
	if (!set || !set->is_subtype_of(this))
		return std::nullopt;
	// Set is the powerset constructor. Widening its ordinal domain preserves
	// every member and therefore needs no element-wise value conversion.
	// The emitter may still copy between distinct target-specific carriers.
	return direct_conversion();
}

static bool interface_is_or_extends(
    const InterfaceType* source,
    const InterfaceType* target) {
	if (source == target)
		return true;
	for (InterfaceType* parent :
	     source->super_interfaces)
		if (interface_is_or_extends(parent, target))
			return true;
	return false;
}

bool InterfaceType::is_subtype_of(
    const Type* target) const {
	auto target_interface =
	    dynamic_cast<const InterfaceType*>(target);
	return target_interface &&
	       interface_is_or_extends(
		   this, target_interface);
}

static bool class_implements_interface(
    const ClassType* source,
    const InterfaceType* target) {
	for (const ClassType* current = source;
	     current; current = current->super)
		for (InterfaceType* implemented :
		     current->implemented_interfaces)
			if (interface_is_or_extends(
				implemented, target))
				return true;
	return false;
}

bool ClassType::is_subtype_of(
    const Type* target) const {
	if (auto target_class =
		dynamic_cast<const ClassType*>(target)) {
		for (const ClassType* current = this;
		     current; current = current->super)
			if (current == target_class)
				return true;
		return false;
	}
	if (auto target_interface =
		dynamic_cast<const InterfaceType*>(target))
		return class_implements_interface(
		    this, target_interface);
	return false;
}

bool ObjectType::is_subtype_of(
    const Type* target) const {
	auto target_object =
	    dynamic_cast<const ObjectType*>(target);
	if (!target_object)
		return false;
	for (const ObjectType* current = this;
	     current; current = current->super)
		if (current == target_object)
			return true;
	return false;
}

static unsigned class_inheritance_distance(
    const ClassType* source,
    const ClassType* target) {
	unsigned distance = 0;
	for (const ClassType* current = source;
	     current; current = current->super, ++distance)
		if (current == target)
			return distance;
	return std::numeric_limits<unsigned>::max();
}

static unsigned object_inheritance_distance(
    const ObjectType* source,
    const ObjectType* target) {
	unsigned distance = 0;
	for (const ObjectType* current = source;
	     current; current = current->super, ++distance)
		if (current == target)
			return distance;
	return std::numeric_limits<unsigned>::max();
}

std::optional<ValueConversion>
ClassType::value_conversion_from(
    const Type* source) const {
	auto source_class =
	    dynamic_cast<const ClassType*>(source);
	if (!source_class ||
	    !source_class->is_subtype_of(this))
		return std::nullopt;
	return implicit_conversion(
	    class_inheritance_distance(source_class, this));
}

std::optional<ValueConversion>
InterfaceType::value_conversion_from(
    const Type* source) const {
	if (!source || !source->is_subtype_of(this))
		return std::nullopt;
	return implicit_conversion(1);
}

std::optional<ValueConversion>
ObjectType::value_conversion_from(
    const Type* source) const {
	auto source_object =
	    dynamic_cast<const ObjectType*>(source);
	if (!source_object ||
	    !source_object->is_subtype_of(this))
		return std::nullopt;
	return implicit_conversion(
	    object_inheritance_distance(
		source_object, this));
}

std::optional<ValueConversion>
ClassRefType::value_conversion_from(
    const Type* source) const {
	auto source_ref =
	    dynamic_cast<const ClassRefType*>(source);
	if (!source_ref)
		return std::nullopt;
	if (source_ref->target == target)
		return direct_conversion();
	if (source_ref->target &&
	    source_ref->target->is_subtype_of(target)) {
		auto source_class =
		    dynamic_cast<const ClassType*>(
			source_ref->target);
		auto target_class =
		    dynamic_cast<const ClassType*>(target);
		unsigned distance =
		    source_class && target_class
			? class_inheritance_distance(
			      source_class, target_class)
			: 1;
		return implicit_conversion(distance);
	}
	return std::nullopt;
}

std::optional<ValueConversion>
PointerType::value_conversion_from(
    const Type* source) const {
	auto source_pointer =
	    dynamic_cast<const PointerType*>(source);
	if (!source_pointer)
		return std::nullopt;
	if (source_pointer->item_type == item_type)
		return direct_conversion();
	if (source_pointer->is_untyped() || is_untyped())
		return implicit_conversion(20);
	auto source_object =
	    dynamic_cast<const ObjectType*>(
		source_pointer->item_type);
	auto target_object =
	    dynamic_cast<const ObjectType*>(item_type);
	if (source_object && target_object &&
	    source_object->is_subtype_of(target_object))
		return implicit_conversion(
		    object_inheritance_distance(
			source_object, target_object));
	return std::nullopt;
}

struct FoldedOrdinalValue {
	bool negative;
	uint64_t magnitude;
};

static std::optional<FoldedOrdinalValue>
fold_ordinal_value(Node* node) {
	if (!node)
		return std::nullopt;
	ConstEvalContext ctx;
	ConstEvalResult folded = node->const_eval(ctx);
	if (folded.kind !=
	    ConstEvalResult::Kind::Success)
		return std::nullopt;
	if (auto integer =
		dynamic_cast<Integer*>(folded.node))
		return FoldedOrdinalValue{
		    integer->negative, integer->value};
	if (auto member =
		dynamic_cast<EnumMemberRef*>(folded.node)) {
		if (member->value < 0)
			return FoldedOrdinalValue{
			    true,
			    static_cast<uint64_t>(
				-(member->value + 1)) +
				1};
		return FoldedOrdinalValue{
		    false,
		    static_cast<uint64_t>(member->value)};
	}
	return std::nullopt;
}

static int compare_folded_ordinals(
    const FoldedOrdinalValue& a,
    const FoldedOrdinalValue& b) {
	if (a.negative != b.negative)
		return a.negative ? -1 : 1;
	if (a.magnitude == b.magnitude)
		return 0;
	if (a.negative)
		return a.magnitude > b.magnitude
			   ? -1
			   : 1;
	return a.magnitude < b.magnitude
		   ? -1
		   : 1;
}

namespace {
enum class OrdinalDomainFamily {
	Integer,
	Character,
	Enumeration,
};

struct OrdinalDomain {
	OrdinalDomainFamily family;
	const Type* nominal_root = nullptr;
	FoldedOrdinalValue lower;
	FoldedOrdinalValue upper;
};

static std::optional<OrdinalDomain>
ordinal_domain(const Type* type) {
	if (!type)
		return std::nullopt;
	if (auto range =
		dynamic_cast<const SubrangeType*>(type)) {
		auto lower =
		    fold_ordinal_value(range->lower_bound);
		auto upper =
		    fold_ordinal_value(range->upper_bound);
		if (!lower || !upper)
			return std::nullopt;
		if (dynamic_cast<const EnumType*>(
			range->base_type))
			return OrdinalDomain{
			    OrdinalDomainFamily::Enumeration,
			    range->base_type, *lower, *upper};
		if (range->base_type == char_type())
			return OrdinalDomain{
			    OrdinalDomainFamily::Character,
			    char_type(), *lower, *upper};
		OrdinalBounds bounds;
		if (integer_bounds(
			range->base_type, &bounds))
			return OrdinalDomain{
			    OrdinalDomainFamily::Integer,
			    nullptr, *lower, *upper};
		return std::nullopt;
	}
	if (auto intrinsic =
		dynamic_cast<const IntrinsicType*>(
		    type);
	    intrinsic && intrinsic->rank &&
	    intrinsic->ordinal_bounds) {
		const OrdinalBounds& bounds =
		    *intrinsic->ordinal_bounds;
		return OrdinalDomain{
		    OrdinalDomainFamily::Integer,
		    nullptr,
		    FoldedOrdinalValue{
			bounds.signed_type,
			bounds.signed_type
			    ? bounds.min_magnitude
			    : 0},
		    FoldedOrdinalValue{
			false, bounds.max_positive}};
	}
	if (type == char_type()) {
		OrdinalBounds bounds;
		if (!intrinsic_ordinal_bounds(
			char_type(), &bounds))
			return std::nullopt;
		return OrdinalDomain{
		    OrdinalDomainFamily::Character,
		    char_type(),
		    FoldedOrdinalValue{false, 0},
		    FoldedOrdinalValue{
			false, bounds.max_positive}};
	}
	if (auto enumeration =
		dynamic_cast<const EnumType*>(type)) {
		const auto* lower =
		    enumeration->min_member();
		const auto* upper =
		    enumeration->max_member();
		if (!lower || !upper)
			return std::nullopt;
		auto as_folded =
		    [](int64_t value) {
			    if (value < 0)
				    return FoldedOrdinalValue{
					true,
					static_cast<uint64_t>(
					    -(value + 1)) +
					    1};
			    return FoldedOrdinalValue{
				false,
				static_cast<uint64_t>(value)};
		    };
		return OrdinalDomain{
		    OrdinalDomainFamily::Enumeration,
		    type, as_folded(lower->value),
		    as_folded(upper->value)};
	}
	return std::nullopt;
}

static bool ordinal_domain_is_subset(
    const Type* source, const Type* target) {
	auto source_domain = ordinal_domain(source);
	auto target_domain = ordinal_domain(target);
	if (!source_domain || !target_domain ||
	    source_domain->family !=
		target_domain->family)
		return false;
	if (source_domain->family !=
		OrdinalDomainFamily::Integer &&
	    source_domain->nominal_root !=
		target_domain->nominal_root)
		return false;
	return compare_folded_ordinals(
		   target_domain->lower,
		   source_domain->lower) <= 0 &&
	       compare_folded_ordinals(
		   target_domain->upper,
		   source_domain->upper) >= 0;
}

static bool ordinal_domains_are_compatible(
    const Type* a, const Type* b) {
	auto a_domain = ordinal_domain(a);
	auto b_domain = ordinal_domain(b);
	if (!a_domain || !b_domain ||
	    a_domain->family != b_domain->family)
		return false;
	return a_domain->family ==
		   OrdinalDomainFamily::Integer ||
	       a_domain->nominal_root ==
		   b_domain->nominal_root;
}
} // namespace

bool IntrinsicType::is_subtype_of(
    const Type* target) const {
	if (this == target)
		return true;
	// Only predefined integer-family types use range subtyping. Char has an
	// ordinal range too, but it remains a distinct nominal ordinal family.
	return rank &&
	       ordinal_domain_is_subset(this, target);
}

bool IntrinsicType::
    same_cxx_carrier_definition_as(
	const Type* other) const {
	auto intrinsic =
	    dynamic_cast<const IntrinsicType*>(
		other);
	return intrinsic && carrier &&
	       intrinsic->carrier &&
	       carrier == intrinsic->carrier;
}

bool SubrangeType::is_subtype_of(
    const Type* target) const {
	return this == target ||
	       ordinal_domain_is_subset(this, target);
}

bool FixedSetType::is_subtype_of(
    const Type* target) const {
	if (this == target)
		return true;
	auto set =
	    dynamic_cast<const FixedSetType*>(target);
	return set &&
	       item_type->is_subtype_of(
		   set->item_type);
}

std::optional<ValueConversion>
SubrangeType::value_conversion_from(
    const Type* source) const {
	if (auto source_range =
		dynamic_cast<const SubrangeType*>(source)) {
		if (!ordinal_domains_are_compatible(
			source_range, this))
			return std::nullopt;
		return source_range->is_subtype_of(this)
			   ? std::optional<ValueConversion>{
				 direct_conversion()}
			   : std::optional<ValueConversion>{implicit_conversion(200)};
	}
	if (ordinal_domains_are_compatible(
		source, this))
		return source->is_subtype_of(this)
			   ? std::optional<ValueConversion>{
				 direct_conversion()}
			   : std::optional<ValueConversion>{implicit_conversion(200)};
	if (source == base_type)
		return implicit_conversion(200);
	auto base_conversion =
	    base_type->value_conversion_from(source);
	if (base_conversion)
		return implicit_conversion(
		    200 + base_conversion->distance);
	return std::nullopt;
}

std::optional<ValueConversion>
RoutineType::value_conversion_from(
    const Type* source) const {
	auto routine =
	    dynamic_cast<const RoutineType*>(source);
	if (routine &&
	    accepts_routine_value_from(routine))
		return direct_conversion();
	return std::nullopt;
}

bool RoutineType::same_parameter_and_result_types_as(
    const RoutineType* other) const {
	if (!other ||
	    return_type != other->return_type ||
	    formals.size() != other->formals.size())
		return false;
	for (size_t i = 0; i < formals.size(); ++i) {
		if (formals[i].mode !=
			other->formals[i].mode ||
		    formals[i].ty !=
			other->formals[i].ty)
			return false;
	}
	return true;
}

bool RoutineType::same_signature_as(
    const RoutineType* other) const {
	return other && kind == other->kind &&
	       same_parameter_and_result_types_as(other);
}

bool RoutineType::same_overload_signature_as(
    const RoutineType* other) const {
	if (!other ||
	    formals.size() != other->formals.size())
		return false;
	for (size_t i = 0; i < formals.size(); ++i)
		if (formals[i].ty !=
		    other->formals[i].ty)
			return false;
	return true;
}

bool RoutineType::accepts_routine_value_from(
    const RoutineType* source) const {
	if (!source)
		return false;
	auto value_kind = [](RoutineKind kind) {
		// A class method is declared on m_meta, but a bound reference carries
		// that metaclass receiver in the same two-word representation as every
		// other `procedure of object`.
		return kind == CLASS_METHOD
			   ? METHOD
			   : kind;
	};
	RoutineKind source_kind =
	    value_kind(source->kind);
	RoutineKind target_kind =
	    value_kind(kind);
	if (source_kind != target_kind ||
	    (target_kind != ROUTINE &&
	     target_kind != METHOD))
		return false;
	return same_parameter_and_result_types_as(
	    source);
}

bool RoutineType::same_cxx_parameter_list_as(
    const RoutineType* other) const {
	if (!other ||
	    formals.size() != other->formals.size())
		return false;
	auto carrier_mode =
	    [](ParamMode mode) {
		    // `var` and `out` both emit T&. Const emits const T&, while a
		    // value parameter emits T. These are C++ signature categories,
		    // not Pascal parameter-mode compatibility.
		    switch (mode) {
		    case ParamMode::Value:
			    return 0;
		    case ParamMode::Const:
			    return 1;
		    case ParamMode::Var:
		    case ParamMode::Out:
			    return 2;
		    }
		    return -1;
	    };
	for (size_t i = 0; i < formals.size(); ++i) {
		const Parameter& a = formals[i];
		const Parameter& b = other->formals[i];
		if (carrier_mode(a.mode) !=
		    carrier_mode(b.mode))
			return false;
		if (a.ty == unknown_type() ||
		    b.ty == unknown_type()) {
			// Omitted-type const and mutable formals lower to two explicit
			// storage-view carriers rather than to an arbitrary T or T&.
			if (a.ty != b.ty)
				return false;
			continue;
		}
		if (!a.ty->same_cxx_carrier_as(
			b.ty))
			return false;
	}
	return true;
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

const char* ShortStringType::diagnostic_kind() const {
	return "shortstring";
}

void ShortStringType::collect_diagnostic_edges(ErrorLetContext*) const {
}

void ShortStringType::print_diagnostic_definition(
    ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "capacity: " << static_cast<unsigned>(capacity);
}

// Aggregate frames are indexed for member names by add_frame_edge(), but
// indexing frames is deliberately name-evidence-only. Aggregate type bodies
// print member type refs, so the owning aggregate Type explicitly contributes
// the value-entry Type* edges through this helper. Do not move this discovery
// into ErrorLetContext::index_frame().
static void add_frame_value_type_edges(ErrorLetContext* ctx, const Frame* frame) {
	if (!frame)
		return;
	for (const auto& item :
	     frame->value_declarations()) {
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

const char* TypedFileType::diagnostic_kind() const {
	return "file";
}
void TypedFileType::collect_diagnostic_edges(
    ErrorLetContext* ctx) const {
	ctx->add_type_edge(item_type);
}
void TypedFileType::print_diagnostic_definition(
    ErrorLetContext* ctx, std::ostringstream& out,
    unsigned indent) const {
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

static void collect_variant_diagnostic_edges(
    ErrorLetContext* ctx, const VariantPart* variant) {
	if (!variant)
		return;
	if (variant->selector_type)
		ctx->add_type_edge(variant->selector_type);
	for (const auto& arm : variant->arms) {
		for (const auto& field : arm.fields)
			ctx->add_type_edge(field.ty);
		collect_variant_diagnostic_edges(ctx, arm.variant);
	}
}

static void print_variant_diagnostic_definition(
    ErrorLetContext* ctx, std::ostringstream& out,
    unsigned indent, const VariantPart* variant) {
	if (!variant)
		return;
	ctx->indent(out, indent);
	out << "case ";
	if (variant->has_selector)
		out << variant->selector_name << ": ";
	out << ctx->known_type_ref(variant->selector_type) << " of\n";
	for (size_t i = 0; i < variant->arms.size(); ++i) {
		ctx->indent(out, indent + 1);
		out << "arm " << (i + 1) << ":\n";
		for (const auto& field : variant->arms[i].fields) {
			ctx->indent(out, indent + 2);
			out << (field.pas_name.empty()
				    ? "<field>"
				    : field.pas_name)
			    << ": " << ctx->known_type_ref(field.ty)
			    << ";\n";
		}
		print_variant_diagnostic_definition(
		    ctx, out, indent + 2,
		    variant->arms[i].variant);
	}
}

const char* RecordType::diagnostic_kind() const { return "record"; }
void RecordType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
	collect_variant_diagnostic_edges(ctx, variant);
}
void RecordType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->print_frame_members(out, children, indent + 1);
	print_variant_diagnostic_definition(
	    ctx, out, indent + 1, variant);
	ctx->indent(out, indent);
	out << "end";
}
void RecordType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const { out << " ... end"; }

const char* PackedRecordType::diagnostic_kind() const { return "packed record"; }
void PackedRecordType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
	collect_variant_diagnostic_edges(ctx, variant);
}
void PackedRecordType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->print_frame_members(out, children, indent + 1);
	print_variant_diagnostic_definition(
	    ctx, out, indent + 1, variant);
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
	ctx->add_value_edge(class_constructor);
	ctx->add_value_edge(class_destructor);
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
}
void ClassType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	if (is_abstract) {
		ctx->indent(out, indent + 1);
		out << "abstract: yes\n";
	}
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
	if (class_constructor) {
		ctx->indent(out, indent + 1);
		out << "class constructor: "
		    << ctx->known_value_ref(class_constructor) << "\n";
	}
	if (class_destructor) {
		ctx->indent(out, indent + 1);
		out << "class destructor: "
		    << ctx->known_value_ref(class_destructor) << "\n";
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
void PointerType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	if (item_type)
		ctx->add_type_edge(item_type);
}
void PointerType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	if (item_type)
		out << "to: " << ctx->known_type_ref(item_type);
	else
		out << "untyped";
}
void PointerType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << (item_type ? "to: ..." : "untyped");
}

const char* ModuleType::diagnostic_kind() const { return "module"; }
void ModuleType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(
	    children, DiagnosticFrameUse::ModuleMembers);
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
	else if (kind == CLASS_CONSTRUCTOR)
		out << "class_constructor";
	else if (kind == CLASS_DESTRUCTOR)
		out << "class_destructor";
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
