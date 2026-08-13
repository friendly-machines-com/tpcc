#include "types.h"
#include "builtins.h"
#include "cst.h"
#include "evaluator.h"
#include "frame.h"
#include "numeric_constants.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <set>
#include <utility>

Type::Type(SourceLocation source_location) : source_location(std::move(source_location)) {
}

std::optional<ValueConversion> Type::value_conversion_from(const Type*) const {
	return std::nullopt;
}

std::optional<ValueConversion> Type::destination_conversion_from(const Type* source) const {
	// Most types have no extra narrowing representation operation. Their
	// complete predefined assignment relation is the ordinary relation.
	return value_conversion_from(source);
}

std::optional<AssignmentConversion> Type::assignment_conversion_from(const Type* source) const {
	if (auto ordinary = value_conversion_from(source)) {
		return AssignmentConversion{
		    ordinary->kind == ValueConversionClass::Direct ? AssignmentConversionClass::Equal : AssignmentConversionClass::Widening,
		    ordinary->distance,
		};
	} else if (auto destination = destination_conversion_from(source)) {
		// The ordinary relation was already absent. Any additional edge in
		// the complete destination relation is therefore assignment-only
		// narrowing, irrespective of the legacy ValueConversionClass spelling.
		return AssignmentConversion{AssignmentConversionClass::Narrowing, destination->distance};
	}
	return std::nullopt;
}

bool Type::is_subtype_of(const Type* target) const {
	return this == target;
}

bool Type::same_formal_contract_as(const Type* other) const {
	return this == other;
}

bool Type::same_cxx_carrier_as(const Type* other) const {
	return other && (this == other || same_cxx_carrier_definition_as(other) || other->same_cxx_carrier_definition_as(this));
}

bool Type::same_cxx_carrier_definition_as(const Type*) const {
	// Records, packed records, objects, classes, interfaces, and enums are
	// nominal C++ declarations. Type* identity in the public wrapper is their
	// complete carrier relation.
	return false;
}

IncompleteType::IncompleteType(SourceLocation source_location, std::string name) : Type(std::move(source_location)), name(std::move(name)), resolved(nullptr) {
}

DistinctType::DistinctType(SourceLocation source_location, std::string cxx_name, Type* base_type) : Type(std::move(source_location)), cxx_name(std::move(cxx_name)), base_type(base_type) {
	assert(!this->cxx_name.empty());
	assert(this->base_type);
}

Type* distinct_storage_type(Type* type) {
	while (auto distinct = dynamic_cast<DistinctType*>(type)) {
		type = distinct->base_type;
	}
	return type;
}

const Type* distinct_storage_type(const Type* type) {
	while (auto distinct = dynamic_cast<const DistinctType*>(type)) {
		type = distinct->base_type;
	}
	return type;
}

EnumType::EnumType(SourceLocation source_location) : Type(std::move(source_location)), cxx_name("") {
}

EnumType::EnumType(SourceLocation source_location, std::string p_cxx_name, std::string a, std::string b, unsigned p_carrier_bits, bool p_carrier_signed) : Type(std::move(source_location)), cxx_name(std::move(p_cxx_name)), carrier_bits(p_carrier_bits), carrier_signed(p_carrier_signed) {
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
	if (members.empty()) {
		return nullptr;
	}
	return &*std::min_element(members.begin(), members.end(), [](const Member& a, const Member& b) { return a.value < b.value; });
}

const EnumType::Member* EnumType::max_member() const {
	if (members.empty()) {
		return nullptr;
	}
	return &*std::max_element(members.begin(), members.end(), [](const Member& a, const Member& b) { return a.value < b.value; });
}

FixedArrayType::FixedArrayType(SourceLocation source_location, Type* bounds, OrdinalRange range, Type* item_type) : Type(std::move(source_location)) {
	this->bounds = bounds;
	this->range = range;
	this->item_type = item_type;
}

DynamicArrayType::DynamicArrayType(SourceLocation source_location, Type* item_type) : Type(std::move(source_location)), item_type(item_type) {
}

OpenArrayType::OpenArrayType(SourceLocation source_location, Type* item_type) : Type(std::move(source_location)), item_type(item_type) {
}

ShortStringType::ShortStringType(SourceLocation source_location, uint8_t capacity) : Type(std::move(source_location)), capacity(capacity) {
	assert(capacity != 0);
}

Type* ShortStringType::sequence_element_type() const {
	return char_type();
}

Type* ShortStringType::sequence_index_type() const {
	return integer_type();
}

Type* ShortStringType::sequence_length_type() const {
	return byte_type();
}

Type* FixedArrayType::sequence_length_type() const {
	return sizeint_type();
}

bool FixedArrayType::has_managed_lifetime() const {
	return item_type && item_type->has_managed_lifetime();
}

bool FixedArrayType::contains_file_state() const {
	return item_type && item_type->contains_file_state();
}

Type* DynamicArrayType::sequence_index_type() const {
	return sizeint_type();
}

Type* DynamicArrayType::sequence_length_type() const {
	return sizeint_type();
}

Type* OpenArrayType::sequence_index_type() const {
	return sizeint_type();
}

Type* OpenArrayType::sequence_length_type() const {
	return sizeint_type();
}

FixedSetType::FixedSetType(SourceLocation source_location, Type* item_type) : Type(std::move(source_location)) {
	this->item_type = item_type;
}

TypedFileType::TypedFileType(SourceLocation source_location, Type* item_type) : Type(std::move(source_location)), item_type(item_type) {
}

PointerType::PointerType(SourceLocation source_location, Type* item_type, std::string cxx_name) : Type(std::move(source_location)), item_type(item_type), cxx_name(std::move(cxx_name)) {
}

RecordType::RecordType(SourceLocation source_location, Frame* children) : Type(std::move(source_location)) {
	this->children = children;
}

PackedRecordType::PackedRecordType(SourceLocation source_location, Frame* children) : Type(std::move(source_location)) {
	this->children = children;
}

ClassType::ClassType(SourceLocation source_location, Frame* children, std::vector<InterfaceType*> implemented_interfaces, ClassType* super) : Type(std::move(source_location)) {
	this->children = children;
	this->implemented_interfaces = std::move(implemented_interfaces);
	this->super = super;
}

InterfaceType::InterfaceType(SourceLocation source_location, Frame* children, std::vector<InterfaceType*> super_interfaces) : Type(std::move(source_location)) {
	this->children = children;
	this->super_interfaces = std::move(super_interfaces);
}

InterfaceType::InterfaceType(SourceLocation source_location, std::string cxx_name, Frame* children, std::vector<InterfaceType*> super_interfaces) : Type(std::move(source_location)) {
	this->cxx_name = std::move(cxx_name);
	this->children = children;
	this->super_interfaces = std::move(super_interfaces);
}

ObjectType::ObjectType(SourceLocation source_location, Frame* children, ObjectType* super) : Type(std::move(source_location)) {
	this->children = children;
	this->super = super;
	this->needs_vmt = super && super->needs_vmt;
}

static bool variant_has_managed_lifetime(const VariantPart* variant) {
	if (!variant) {
		return false;
	}
	if (variant->selector_type && variant->selector_type->has_managed_lifetime()) {
		return true;
	}
	for (const VariantArm& arm : variant->arms) {
		for (const AggregateField& field : arm.fields) {
			if (field.ty && field.ty->has_managed_lifetime()) {
				return true;
			}
		}
		if (variant_has_managed_lifetime(arm.variant)) {
			return true;
		}
	}
	return false;
}

static bool variant_contains_file_state(const VariantPart* variant) {
	if (!variant) {
		return false;
	}
	if (variant->selector_type && variant->selector_type->contains_file_state()) {
		return true;
	}
	for (const VariantArm& arm : variant->arms) {
		for (const AggregateField& field : arm.fields) {
			if (field.ty && field.ty->contains_file_state()) {
				return true;
			}
		}
		if (variant_contains_file_state(arm.variant)) {
			return true;
		}
	}
	return false;
}

bool RecordType::has_managed_lifetime() const {
	for (const AggregateField& field : fields) {
		if (field.ty && field.ty->has_managed_lifetime()) {
			return true;
		}
	}
	return variant_has_managed_lifetime(variant);
}

bool RecordType::contains_file_state() const {
	for (const AggregateField& field : fields) {
		if (field.ty && field.ty->contains_file_state()) {
			return true;
		}
	}
	return variant_contains_file_state(variant);
}

bool PackedRecordType::has_managed_lifetime() const {
	for (const AggregateField& field : fields) {
		if (field.ty && field.ty->has_managed_lifetime()) {
			return true;
		}
	}
	return variant_has_managed_lifetime(variant);
}

bool PackedRecordType::contains_file_state() const {
	for (const AggregateField& field : fields) {
		if (field.ty && field.ty->contains_file_state()) {
			return true;
		}
	}
	return variant_contains_file_state(variant);
}

bool ObjectType::has_managed_lifetime() const {
	if (super && super->has_managed_lifetime()) {
		return true;
	}
	if (!children) {
		return false;
	}
	for (const auto& declaration : children->value_declarations()) {
		auto slot = dynamic_cast<StorageSlot*>(declaration.second.value);
		if (slot && slot->kind == StorageSlot::Kind::AggregateMember && slot->ty && slot->ty->has_managed_lifetime()) {
			return true;
		}
	}
	return false;
}

bool ObjectType::contains_file_state() const {
	if (super && super->contains_file_state()) {
		return true;
	}
	for (const AggregateField& field : fields) {
		if (field.ty && field.ty->contains_file_state()) {
			return true;
		}
	}
	return false;
}

ModuleType::ModuleType(SourceLocation source_location, Frame* children) : Type(std::move(source_location)) {
	this->children = children;
}

UnitType::UnitType(SourceLocation source_location) : Type(std::move(source_location)) {
}

UntypedIntegerType::UntypedIntegerType(SourceLocation source_location) : Type(std::move(source_location)) {
}

UntypedRealType::UntypedRealType(SourceLocation source_location) : Type(std::move(source_location)) {
}

RoutineType::RoutineType(SourceLocation source_location, std::vector<Parameter> formals, Type* return_type, RoutineKind kind) : Type(std::move(source_location)) {
	this->formals = std::move(formals);
	this->return_type = return_type;
	this->kind = kind;
}

bool ShortStringType::same_cxx_carrier_definition_as(const Type* other) const {
	auto string = dynamic_cast<const ShortStringType*>(other);
	return string && capacity == string->capacity;
}

bool FixedArrayType::same_cxx_carrier_definition_as(const Type* other) const {
	auto array = dynamic_cast<const FixedArrayType*>(other);
	if (!array || range.length != array->range.length || range.lower_ordinal.negative != array->range.lower_ordinal.negative || range.lower_ordinal.magnitude != array->range.lower_ordinal.magnitude || !item_type->same_cxx_carrier_as(array->item_type)) {
		return false;
	}
	Type* lower_type = range.lower_bound ? range.lower_bound->ty : nullptr;
	Type* other_lower_type = array->range.lower_bound ? array->range.lower_bound->ty : nullptr;
	return lower_type && other_lower_type && lower_type->same_cxx_carrier_as(other_lower_type);
}

bool DynamicArrayType::same_cxx_carrier_definition_as(const Type* other) const {
	auto array = dynamic_cast<const DynamicArrayType*>(other);
	return array && item_type->same_cxx_carrier_as(array->item_type);
}

bool OpenArrayType::same_formal_contract_as(const Type* other) const {
	auto array = dynamic_cast<const OpenArrayType*>(other);
	// Each `array of T` formal is a fresh type constructor occurrence, but
	// repeated declarations of one callable still have the same Pascal
	// signature when their element Type is identical. This relation is only
	// declaration-contract equality; it does not make open-array values
	// nominally identical.
	return array && item_type == array->item_type;
}

bool OpenArrayType::same_cxx_carrier_definition_as(const Type* other) const {
	auto array = dynamic_cast<const OpenArrayType*>(other);
	return array && item_type->same_cxx_carrier_as(array->item_type);
}

bool FixedSetType::same_cxx_carrier_definition_as(const Type* other) const {
	auto set = dynamic_cast<const FixedSetType*>(other);
	return set && item_type->same_cxx_carrier_as(set->item_type);
}

bool TypedFileType::same_cxx_carrier_definition_as(const Type* other) const {
	auto file = dynamic_cast<const TypedFileType*>(other);
	return file && item_type->same_cxx_carrier_as(file->item_type);
}

bool ClassRefType::same_cxx_carrier_definition_as(const Type* other) const {
	auto reference = dynamic_cast<const ClassRefType*>(other);
	// m_classref<T> contains the nominal target as a template argument.
	return reference && target == reference->target;
}

bool PointerType::same_cxx_carrier_definition_as(const Type* other) const {
	auto pointer = dynamic_cast<const PointerType*>(other);
	if (!pointer || is_untyped() != pointer->is_untyped()) {
		return false;
	}
	if (is_untyped()) {
		return cxx_name == pointer->cxx_name;
	}
	return item_type->same_cxx_carrier_as(pointer->item_type);
}

bool RoutineType::same_cxx_carrier_definition_as(const Type* other) const {
	auto routine = dynamic_cast<const RoutineType*>(other);
	return routine && kind == routine->kind && return_type->same_cxx_carrier_as(routine->return_type) && same_cxx_parameter_list_as(routine);
}

namespace {

bool checked_add_u64(uint64_t a, uint64_t b, uint64_t* result) {
	if (b > std::numeric_limits<uint64_t>::max() - a) {
		return false;
	}
	*result = a + b;
	return true;
}

bool checked_multiply_u64(uint64_t a, uint64_t b, uint64_t* result) {
	if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) {
		return false;
	}
	*result = a * b;
	return true;
}

bool align_up_u64(uint64_t value, uint64_t alignment, uint64_t* result) {
	if (alignment == 0) {
		return false;
	}
	const uint64_t remainder = value % alignment;
	return remainder == 0 ? (*result = value, true) : checked_add_u64(value, alignment - remainder, result);
}

std::optional<TypeLayout> type_layout_impl(bool packed_container, Type* ty, std::set<Type*>& visiting);

struct SequentialLayout {
	uint64_t offset = 0;
	uint64_t alignment = 1;
	std::vector<AggregateFieldLayout> fields;
};

// not packed.
bool append_aligned_field(SequentialLayout& layout, StorageSlot* slot, Type* ty, std::set<Type*>& visiting) {
	auto field_layout = type_layout_impl(false, ty, visiting);
	if (!field_layout) {
		return false;
	}
	uint64_t offset;
	if (!align_up_u64(layout.offset, field_layout->alignment, &offset)) {
		return false;
	}
	uint64_t end;
	if (!checked_add_u64(offset, field_layout->size, &end)) {
		return false;
	}
	layout.fields.push_back(AggregateFieldLayout{slot, ty, offset, field_layout->size});
	layout.offset = end;
	layout.alignment = std::max(layout.alignment, field_layout->alignment);
	return true;
}

bool append_aligned_variant(SequentialLayout& layout, VariantPart* variant, std::set<Type*>& visiting) {
	if (!variant) {
		return true;
	}
	if (variant->has_selector && !append_aligned_field(layout, variant->selector_slot, variant->selector_type, visiting)) {
		return false;
	}
	if (variant->arms.empty()) {
		return true;
	}

	uint64_t union_size = 0;
	uint64_t union_alignment = 1;
	std::vector<std::vector<AggregateFieldLayout>> arm_fields;
	for (const auto& arm : variant->arms) {
		SequentialLayout arm_layout;
		for (const auto& field : arm.fields) {
			if (!append_aligned_field(arm_layout, field.slot, field.ty, visiting)) {
				return false;
			}
		}
		if (!append_aligned_variant(arm_layout, arm.variant, visiting)) {
			return false;
		}

		uint64_t arm_size;
		if (!align_up_u64(arm_layout.offset == 0 ? 1 : arm_layout.offset, arm_layout.alignment, &arm_size)) {
			return false;
		}
		union_size = std::max(union_size, arm_size);
		union_alignment = std::max(union_alignment, arm_layout.alignment);
		arm_fields.push_back(std::move(arm_layout.fields));
	}

	uint64_t union_offset;
	if (!align_up_u64(layout.offset, union_alignment, &union_offset) || !checked_add_u64(union_offset, union_size, &layout.offset)) {
		return false;
	}
	layout.alignment = std::max(layout.alignment, union_alignment);
	for (auto& fields : arm_fields) {
		for (auto& field : fields) {
			if (!checked_add_u64(field.offset, union_offset, &field.offset)) {
				return false;
			}
			layout.fields.push_back(field);
		}
	}
	return true;
}

std::optional<RecordLayout> record_layout_impl(RecordType* record, std::set<Type*>& visiting) {
	if (!visiting.insert(record).second) {
		return std::nullopt;
	}

	SequentialLayout fixed;
	for (const auto& field : record->fields) {
		if (!append_aligned_field(fixed, field.slot, field.ty, visiting)) {
			visiting.erase(record);
			return std::nullopt;
		}
	}
	if (!append_aligned_variant(fixed, record->variant, visiting)) {
		visiting.erase(record);
		return std::nullopt;
	}

	uint64_t size;
	if (!align_up_u64(fixed.offset == 0 ? 1 : fixed.offset, fixed.alignment, &size)) {
		visiting.erase(record);
		return std::nullopt;
	}
	visiting.erase(record);
	return RecordLayout{
	    TypeLayout{size, fixed.alignment},
	    std::move(fixed.fields),
	};
}

// Object layout walks super first (recursive), then own source-order fields.
// Like RecordType but with a super pointer and no variant part. The C++
// backend handles vmt/vptr via its own ABI when an object declares virtual
// methods; this layout calculation does not model vptr, only declared fields.
std::optional<TypeLayout> object_layout_impl(ObjectType* object, std::set<Type*>& visiting) {
	if (!visiting.insert(object).second) {
		return std::nullopt;
	}

	SequentialLayout fixed;

	if (object->super) {
		auto super_layout = object_layout_impl(object->super, visiting);
		if (!super_layout) {
			visiting.erase(object);
			return std::nullopt;
		}
		fixed.offset = super_layout->size;
		fixed.alignment = super_layout->alignment;
	}

	for (const auto& field : object->fields) {
		if (!append_aligned_field(fixed, field.slot, field.ty, visiting)) {
			visiting.erase(object);
			return std::nullopt;
		}
	}

	uint64_t size;
	bool ok = align_up_u64(fixed.offset == 0 ? 1 : fixed.offset, fixed.alignment, &size);
	visiting.erase(object);
	return ok ? std::optional<TypeLayout>{TypeLayout{size, fixed.alignment}} : std::nullopt;
}

struct PackedSequentialLayout {
	uint64_t offset = 0;
	std::vector<AggregateFieldLayout> fields;
};

bool append_packed_field(PackedSequentialLayout& layout, StorageSlot* slot, Type* ty, std::set<Type*>& visiting) {
	auto field_layout = type_layout_impl(true, ty, visiting); // FIXME what
	if (!field_layout) {
		return false;
	}
	uint64_t end;
	if (!checked_add_u64(layout.offset, field_layout->size, &end)) {
		return false;
	}
	layout.fields.push_back(AggregateFieldLayout{slot, ty, layout.offset, field_layout->size});
	layout.offset = end;
	return true;
}

bool append_packed_variant(PackedSequentialLayout& layout, VariantPart* variant, std::set<Type*>& visiting) {
	if (!variant) {
		return true;
	}
	if (variant->has_selector && !append_packed_field(layout, variant->selector_slot, variant->selector_type, visiting)) {
		return false;
	}
	if (variant->arms.empty()) {
		return true;
	}

	const uint64_t union_offset = layout.offset;
	uint64_t union_size = 0;
	std::vector<std::vector<AggregateFieldLayout>> arm_fields;
	for (const auto& arm : variant->arms) {
		PackedSequentialLayout arm_layout;
		for (const auto& field : arm.fields) {
			if (!append_packed_field(arm_layout, field.slot, field.ty, visiting)) {
				return false;
			}
		}
		if (!append_packed_variant(arm_layout, arm.variant, visiting)) {
			return false;
		}
		union_size = std::max(union_size, arm_layout.offset);
		arm_fields.push_back(std::move(arm_layout.fields));
	}
	if (!checked_add_u64(union_offset, union_size, &layout.offset)) {
		return false;
	}
	for (auto& fields : arm_fields) {
		for (auto& field : fields) {
			if (!checked_add_u64(field.offset, union_offset, &field.offset)) {
				return false;
			}
			layout.fields.push_back(field);
		}
	}
	return true;
}

std::optional<RecordLayout> packed_record_layout_impl(PackedRecordType* record, std::set<Type*>& visiting) {
	if (!visiting.insert(record).second) {
		return std::nullopt;
	}
	PackedSequentialLayout layout;
	for (const auto& field : record->fields) {
		if (!append_packed_field(layout, field.slot, field.ty, visiting)) {
			visiting.erase(record);
			return std::nullopt;
		}
	}
	if (!append_packed_variant(layout, record->variant, visiting)) {
		visiting.erase(record);
		return std::nullopt;
	}
	visiting.erase(record);
	return RecordLayout{
	    TypeLayout{layout.offset == 0 ? 1 : layout.offset, 1},
	    std::move(layout.fields),
	};
}

std::optional<TypeLayout> type_layout_impl(bool packed_container, Type* ty, std::set<Type*>& visiting) {
	while (auto incomplete = dynamic_cast<IncompleteType*>(ty)) {
		if (!incomplete->resolved) {
			return std::nullopt;
		}
		ty = incomplete->resolved;
	}
	if (auto distinct = dynamic_cast<DistinctType*>(ty)) {
		// FPC's `type Base` changes Pascal identity, not storage layout.
		return type_layout_impl(packed_container, distinct->base_type, visiting);
	} else if (auto intrinsic = dynamic_cast<IntrinsicType*>(ty)) {
		return intrinsic->layout;
	} else if (auto shortstring = dynamic_cast<ShortStringType*>(ty)) {
		return TypeLayout{static_cast<uint64_t>(shortstring->capacity) + 1, 1};
	} else if (auto packed = dynamic_cast<PackedRecordType*>(ty)) {
		auto layout = packed_record_layout_impl(packed, visiting);
		return layout ? std::optional<TypeLayout>{layout->type} : std::nullopt;
	} else if (auto array = dynamic_cast<FixedArrayType*>(ty)) {
		auto item = type_layout_impl(packed_container, array->item_type, visiting);
		if (!item) {
			return std::nullopt;
		}
		if (packed_container && item->alignment != 1) {
			fprintf(stderr, "error: item with alignment != 1 is not allowed inside a packed record.\n");
			abort();
		}
		uint64_t size;
		if (!checked_multiply_u64(item->size, array->range.length, &size)) {
			return std::nullopt;
		}
		return TypeLayout{size, item->alignment};
	} else if (auto subrange = dynamic_cast<SubrangeType*>(ty)) {
		// The emitted carrier is deliberately one base-type member, with
		// generated static assertions enforcing identical size and alignment.
		// Packed-record layout can therefore keep using the Pascal storage
		// layout without duplicating a C++ ABI calculator here.
		return type_layout_impl(packed_container, subrange->base_type, visiting);
	} else if (auto enumeration = dynamic_cast<EnumType*>(ty)) {
		uint64_t bytes = enumeration->carrier_bits / 8;
		return enumeration->carrier_bits != 0 && enumeration->carrier_bits % 8 == 0 ? std::optional<TypeLayout>{TypeLayout{bytes, bytes}} : std::nullopt;
	} else if (packed_container) {
		// The others are not allowed inside packed records.
		return std::nullopt;
	} else if (dynamic_cast<DynamicArrayType*>(ty)) {
		// A dynamic array stores one shared-buffer handle, independent of its
		// element type or current length.
		return TypeLayout{8, 8}; // FIXME: target-dependent, impl-dependent
	} else if (dynamic_cast<OpenArrayType*>(ty)) {
		// An open array is the non-owning data-and-count descriptor passed by
		// open-array formals.
		return TypeLayout{16, 8}; // FIXME: target-dependent, impl-dependent
	} else if (dynamic_cast<FixedSetType*>(ty)) {
		return TypeLayout{24, 8}; // FIXME: target-dependent, impl-dependent
	} else if (dynamic_cast<TypedFileType*>(ty)) {
		return TypeLayout{8, 8}; // FIXME: target-dependent, impl-dependent
	} else if (dynamic_cast<PointerType*>(ty) || dynamic_cast<ClassType*>(ty) || dynamic_cast<InterfaceType*>(ty) || dynamic_cast<ClassRefType*>(ty)) {
		return TypeLayout{8, 8}; // FIXME: target-dependent, impl-dependent
	} else if (auto record = dynamic_cast<RecordType*>(ty)) {
		auto layout = record_layout_impl(record, visiting);
		return layout ? std::optional<TypeLayout>{layout->type} : std::nullopt;
	} else if (auto object = dynamic_cast<ObjectType*>(ty)) {
		return object_layout_impl(object, visiting);
	} else if (auto routine = dynamic_cast<RoutineType*>(ty)) {
		if (routine->kind == METHOD) {
			return TypeLayout{16, 8}; // FIXME: target-dependent
		}
		return TypeLayout{8, 8}; // FIXME: target-dependent
	}
	return std::nullopt;
}

} // namespace

std::optional<TypeLayout> type_layout(bool packed_container, Type* ty) {
	std::set<Type*> visiting;
	return type_layout_impl(packed_container, ty, visiting);
}

std::optional<RecordLayout> record_layout(RecordType* record) {
	std::set<Type*> visiting;
	return record_layout_impl(record, visiting);
}

std::optional<RecordLayout> packed_record_layout(PackedRecordType* record) {
	std::set<Type*> visiting;
	return packed_record_layout_impl(record, visiting);
}

namespace {
static bool predefined_overlay_byte_copyable(const Type* type, std::set<const Type*>& visiting);

static bool predefined_scalar_byte_copyable(const Type* type) {
	if (!type) {
		return false;
	}
	while (auto incomplete = dynamic_cast<const IncompleteType*>(type)) {
		if (!incomplete->resolved) {
			return false;
		}
		type = incomplete->resolved;
	}
	if (auto distinct = dynamic_cast<const DistinctType*>(type)) {
		return predefined_scalar_byte_copyable(distinct->base_type);
	}
	if (auto range = dynamic_cast<const SubrangeType*>(type)) {
		return predefined_scalar_byte_copyable(range->base_type);
	}
	if (auto intrinsic = dynamic_cast<const IntrinsicType*>(type)) {
		if (!intrinsic->carrier) {
			return false;
		}
		switch (*intrinsic->carrier) {
		case IntrinsicCarrier::UInt8:
		case IntrinsicCarrier::Int8:
		case IntrinsicCarrier::UInt16:
		case IntrinsicCarrier::Int16:
		case IntrinsicCarrier::UInt32:
		case IntrinsicCarrier::Int32:
		case IntrinsicCarrier::UInt64:
		case IntrinsicCarrier::Int64:
		case IntrinsicCarrier::Float:
		case IntrinsicCarrier::Double:
		case IntrinsicCarrier::LongDouble:
		case IntrinsicCarrier::Character:
			return true;
		case IntrinsicCarrier::AnsiString:
		case IntrinsicCarrier::Text:
		case IntrinsicCarrier::File:
			return false;
		}
	}
	return dynamic_cast<const EnumType*>(type) ||
	       dynamic_cast<const PointerType*>(type) ||
	       dynamic_cast<const ClassType*>(type) ||
	       dynamic_cast<const InterfaceType*>(type) ||
	       dynamic_cast<const ClassRefType*>(type) ||
	       dynamic_cast<const RoutineType*>(type);
}

static bool predefined_overlay_variant_byte_copyable(const VariantPart* variant, std::set<const Type*>& visiting) {
	if (!variant) {
		return true;
	}
	for (const VariantArm& arm : variant->arms) {
		for (const AggregateField& field : arm.fields) {
			if (!field.ty || !predefined_overlay_byte_copyable(field.ty, visiting)) {
				return false;
			}
		}
		if (!predefined_overlay_variant_byte_copyable(arm.variant, visiting)) {
			return false;
		}
	}
	return true;
}

static bool predefined_overlay_byte_copyable(const Type* type, std::set<const Type*>& visiting) {
	if (!type) {
		return false;
	}
	while (auto incomplete = dynamic_cast<const IncompleteType*>(type)) {
		if (!incomplete->resolved) {
			return false;
		}
		type = incomplete->resolved;
	}
	if (auto intrinsic = dynamic_cast<const IntrinsicType*>(type)) {
		if (!intrinsic->carrier) {
			return false;
		}
		switch (*intrinsic->carrier) {
		case IntrinsicCarrier::UInt8:
		case IntrinsicCarrier::Int8:
		case IntrinsicCarrier::UInt16:
		case IntrinsicCarrier::Int16:
		case IntrinsicCarrier::UInt32:
		case IntrinsicCarrier::Int32:
		case IntrinsicCarrier::UInt64:
		case IntrinsicCarrier::Int64:
		case IntrinsicCarrier::Float:
		case IntrinsicCarrier::Double:
		case IntrinsicCarrier::LongDouble:
		case IntrinsicCarrier::Character:
			return true;
		case IntrinsicCarrier::AnsiString:
		case IntrinsicCarrier::Text:
		case IntrinsicCarrier::File:
			return false;
		}
	} else if (dynamic_cast<const ShortStringType*>(type) || dynamic_cast<const EnumType*>(type) || dynamic_cast<const PointerType*>(type) || dynamic_cast<const ClassType*>(type) || dynamic_cast<const InterfaceType*>(type) || dynamic_cast<const ClassRefType*>(type) || dynamic_cast<const RoutineType*>(type) || dynamic_cast<const PackedRecordType*>(type)) {
		return true;
	} else if (auto range = dynamic_cast<const SubrangeType*>(type)) {
		return predefined_overlay_byte_copyable(range->base_type, visiting);
	}
	if (!visiting.insert(type).second) {
		// A recursive value carrier cannot be laid out without crossing a
		// pointer, which was handled above. Do not let a malformed cycle
		// become an emitted C++ static-assert failure.
		return false;
	}
	bool result = false;
	if (auto array = dynamic_cast<const FixedArrayType*>(type)) {
		result = predefined_overlay_byte_copyable(array->item_type, visiting);
	} else if (auto record = dynamic_cast<const RecordType*>(type)) {
		result = true;
		for (const AggregateField& field : record->fields) {
			if (!field.ty || !predefined_overlay_byte_copyable(field.ty, visiting)) {
				result = false;
				break;
			}
		}
		if (result) {
			result = predefined_overlay_variant_byte_copyable(record->variant, visiting);
		}
	}
	visiting.erase(type);
	return result;
}

static bool predefined_overlay_compatible(const Type* target, const Type* source) {
	if (!target || !source || (!dynamic_cast<const PackedRecordType*>(target) && !dynamic_cast<const PackedRecordType*>(source))) {
		return false;
	}
	std::set<const Type*> visiting;
	if (!predefined_overlay_byte_copyable(target, visiting)) {
		return false;
	}
	visiting.clear();
	if (!predefined_overlay_byte_copyable(source, visiting)) {
		return false;
	}
	auto target_layout = type_layout(false, const_cast<Type*>(target));
	auto source_layout = type_layout(false, const_cast<Type*>(source));
	return target_layout && source_layout && target_layout->size == source_layout->size;
}
} // namespace

bool predefined_byte_array_storage_view(const Type* target, const Type* source) {
	auto array = dynamic_cast<const FixedArrayType*>(target);
	if (!array || array->item_type != byte_type() ||
	    !predefined_scalar_byte_copyable(source)) {
		return false;
	}
	auto target_layout =
	    type_layout(false, const_cast<Type*>(target));
	auto source_layout =
	    type_layout(false, const_cast<Type*>(source));
	return target_layout && source_layout &&
	       target_layout->size == source_layout->size;
}

bool Type::predefined_explicit_conversion_from(const Type* source) const {
	// Explicit syntax includes every one-edge predefined implicit conversion.
	// Calling this virtual destination's existing constructor relation does
	// not search for an intermediate type or a source-defined operator.
	if (!source) {
		return false;
	}
	if (this == source || value_conversion_from(source)) {
		return true;
	}
	if (predefined_byte_array_storage_view(this, source)) {
		return true;
	}
	// Packed records are the language's byte-array overlay carrier. Admit the
	// operation only when both layouts are known and the other carrier is
	// recursively byte-copyable, so an invalid Pascal cast is rejected here
	// rather than escaping as a generated C++ static_assert.
	return predefined_overlay_compatible(this, source);
}

// Integer widening rank; -1 for non-integer types.
static int integer_widening_rank(const Type* ty) {
	for (;;) {
		if (auto distinct = dynamic_cast<const DistinctType*>(ty)) {
			ty = distinct->base_type;
			continue;
		}
		if (auto range = dynamic_cast<const SubrangeType*>(ty)) {
			ty = range->base_type;
			continue;
		}
		break;
	}
	auto it = dynamic_cast<const IntrinsicType*>(ty);
	if (!it) {
		return -1;
	}
	if (!it->rank) {
		return -1;
	}
	return *(it->rank);
}

static bool predefined_ordinal_type(const Type* type) {
	if (type == &untyped_integer_type()) {
		return true;
	}
	for (;;) {
		if (auto distinct = dynamic_cast<const DistinctType*>(type)) {
			type = distinct->base_type;
			continue;
		}
		if (auto range = dynamic_cast<const SubrangeType*>(type)) {
			type = range->base_type;
			continue;
		}
		break;
	}
	if (dynamic_cast<const EnumType*>(type)) {
		return true;
	}
	auto intrinsic = dynamic_cast<const IntrinsicType*>(type);
	return intrinsic && intrinsic->ordinal_bounds.has_value();
}

static bool predefined_integer_family_type(const Type* type) {
	return type == &untyped_integer_type() || integer_widening_rank(type) >= 0;
}

static bool predefined_object_reference_type(const Type* type) {
	return dynamic_cast<const ClassType*>(type) || dynamic_cast<const InterfaceType*>(type);
}

static bool integer_like_bounds(const Type* ty, OrdinalBounds* out) {
	if (integer_bounds(ty, out)) {
		return true;
	}
	if (auto s = dynamic_cast<const SubrangeType*>(ty)) {
		ConstEvalContext ctx;
		ConstEvalResult lower = s->lower_bound->const_eval(ctx);
		ConstEvalResult upper = s->upper_bound->const_eval(ctx);
		if (lower.kind != ConstEvalResult::Kind::Success || upper.kind != ConstEvalResult::Kind::Success) {
			return false;
		}
		auto lo = dynamic_cast<Integer*>(lower.node);
		auto hi = dynamic_cast<Integer*>(upper.node);
		if (!lo || !hi) {
			return false;
		}
		out->signed_type = lo->negative;
		out->min_magnitude = lo->negative ? lo->value : 0;
		out->max_positive = hi->negative ? 0 : hi->value;
		return true;
	}
	return false;
}

static uint64_t saturating_add(uint64_t a, uint64_t b) {
	if (a > UINT64_MAX - b) {
		return UINT64_MAX;
	}
	return a + b;
}

static uint64_t unsigned_abs_diff(uint64_t a, uint64_t b) {
	return a >= b ? a - b : b - a;
}

static uint64_t ordinal_lower_bound_distance(const OrdinalBounds& a, const OrdinalBounds& b) {
	if (a.signed_type && b.signed_type) {
		return unsigned_abs_diff(a.min_magnitude, b.min_magnitude);
	}
	if (a.signed_type) {
		return a.min_magnitude;
	}
	if (b.signed_type) {
		return b.min_magnitude;
	}
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

static int integer_conversion_cost(const Type* from, const Type* to) {
	int rfrom = integer_widening_rank(from), rto = integer_widening_rank(to);
	if (rfrom < 0 || rto < 0) {
		return -1;
	}

	OrdinalBounds from_bounds;
	OrdinalBounds to_bounds;
	if (!integer_like_bounds(from, &from_bounds) || !integer_like_bounds(to, &to_bounds)) {
		return -1;
	}

	// This computes the local cost of a predefined integer assignment edge.
	// The destination constructor admits the edge only when the source is a
	// subtype of the destination; the reverse, narrowing direction is not an
	// implicit edge.
	uint64_t distance = ordinal_lower_bound_distance(from_bounds, to_bounds);
	distance = saturating_add(distance, unsigned_abs_diff(from_bounds.max_positive, to_bounds.max_positive));

	int cost = 10 + bit_width(distance) * 2;
	if (from_bounds.signed_type != to_bounds.signed_type) {
		++cost;
	}
	return cost;
}

static ValueConversion direct_conversion(unsigned distance = 0) {
	return ValueConversion{ValueConversionClass::Direct, distance};
}

static ValueConversion implicit_conversion(unsigned distance = 0) {
	return ValueConversion{ValueConversionClass::Convert, distance};
}

std::optional<ValueConversion> DistinctType::value_conversion_from(const Type* source) const {
	if (!source) {
		return std::nullopt;
	}
	const Type* target_storage = distinct_storage_type(base_type);
	const Type* source_storage = distinct_storage_type(source);
	if (source_storage == target_storage) {
		// `type Base` is a new overload identity, but FPC defines it as
		// representation-compatible with Base and sibling distinct types.
		return direct_conversion();
	}
	return target_storage->value_conversion_from(source_storage);
}

std::optional<ValueConversion> DistinctType::destination_conversion_from(const Type* source) const {
	if (auto ordinary = value_conversion_from(source)) {
		return ordinary;
	}
	const Type* target_storage = distinct_storage_type(base_type);
	return target_storage->destination_conversion_from(distinct_storage_type(source));
}

bool DistinctType::predefined_explicit_conversion_from(const Type* source) const {
	if (!source) {
		return false;
	}
	const Type* target_storage = distinct_storage_type(base_type);
	const Type* source_storage = distinct_storage_type(source);
	return source_storage == target_storage || target_storage->predefined_explicit_conversion_from(source_storage);
}

bool DistinctType::is_subtype_of(const Type* target) const {
	if (this == target) {
		return true;
	}
	const Type* storage = distinct_storage_type(base_type);
	const Type* target_storage = distinct_storage_type(target);
	return storage == target_storage || storage->is_subtype_of(target_storage);
}

bool DistinctType::same_cxx_carrier_definition_as(const Type* other) const {
	return base_type && base_type->same_cxx_carrier_as(distinct_storage_type(other));
}

Type* DistinctType::sequence_element_type() const {
	return base_type->sequence_element_type();
}

Type* DistinctType::array_element_type() const {
	return base_type->array_element_type();
}

Type* DistinctType::sequence_index_type() const {
	return base_type->sequence_index_type();
}

Type* DistinctType::sequence_length_type() const {
	return base_type->sequence_length_type();
}

bool DistinctType::sequence_is_resizable() const {
	return base_type->sequence_is_resizable();
}

bool DistinctType::has_managed_lifetime() const {
	return base_type->has_managed_lifetime();
}

bool DistinctType::contains_file_state() const {
	return base_type->contains_file_state();
}

bool DistinctType::is_reference_type() const {
	return base_type->is_reference_type();
}

std::optional<ValueConversion> IntrinsicType::value_conversion_from(const Type* source_const) const {
	auto source = distinct_storage_type(source_const);
	auto target = this;
	auto source_string = dynamic_cast<const ShortStringType*>(source);
	auto source_range = dynamic_cast<const SubrangeType*>(source);
	int integer_cost = integer_conversion_cost(source, target);
	int source_real = real_semantic_rank(source);
	int target_real = real_semantic_rank(target);
	if (source_const != source && source == target) {
		// Exact identity was already tested by the matcher. A distinct
		// identity over this same carrier is the FPC strong-type direct case,
		// not an integer/real widening conversion.
		return direct_conversion();
	} else if (target == ansistring_type() && source_string) {
		// Managed AnsiString represents every ShortString payload. This is one
		// ordinary assignment edge for every fixed capacity, not a chain
		// through the canonical String[255] declaration.
		return implicit_conversion(source_string->capacity);
	} else if (source == &untyped_integer_type() && integer_widening_rank(target) >= 0) {
		// The expression matcher checks the literal's actual magnitude. At the
		// type level it is a contextual integer value, not another nominal
		// integer definition.
		return direct_conversion();
	} else if (source == &untyped_integer_type() && target_real >= 0) {
		return implicit_conversion(500 + target_real);
	} else if (source == &untyped_real_type() && target_real >= 0) {
		// The expression matcher owns exact decimal materialization and its
		// candidate-local quality. At type level this merely records that an
		// origin may acquire any visible concrete real destination.
		return direct_conversion();
	} else if (source_range && source_range->base_type == target) {
		// Char is a distinct nominal ordinal family, not an unsigned integer
		// widening source or destination. Byte(CharValue) and Char(ByteValue)
		// remain predefined explicit casts, but neither crossing may make a
		// numeric overload viable.
		//
		// A subrange has distinct Pascal identity but uses its declared base
		// representation directly. Passing it to that base formal needs no
		// assignment operator; another containing integer formal requires one
		// widening edge.
		return direct_conversion();
	} else if (integer_cost >= 0 && source->is_subtype_of(target)) {
		// The ordinary portion of integer assignment follows value-set
		// inclusion. The complete assignment query classifies the reverse
		// direction as Narrowing.
		return implicit_conversion(static_cast<unsigned>(integer_cost));
	} else if (source_real >= 0 && target_real >= source_real) {
		// The ordinary portion of real assignment follows the declared
		// precision direction. The complete query classifies the reverse
		// direction as Narrowing; {$R} changes only its emitted check.
		unsigned distance = static_cast<unsigned>(target_real - source_real);
		return implicit_conversion(distance);
	} else if (integer_widening_rank(source) >= 0 && target_real >= 0) {
		return implicit_conversion(500 + static_cast<unsigned>(target_real));
	}
	return std::nullopt;
}

std::optional<ValueConversion> IntrinsicType::destination_conversion_from(const Type* source) const {
	if (auto ordinary = value_conversion_from(source)) {
		return ordinary;
	} else if (const int integer_cost = integer_conversion_cost(source, this); integer_cost >= 0) {
		// A selected ordinal destination may truncate or reinterpret sign.
		// This is the Pascal assignment boundary checked by {$R+} and the
		// Narrowing tier used by value-argument matching.
		return implicit_conversion(static_cast<unsigned>(integer_cost));
	} else if (const int source_real = real_semantic_rank(source), target_real = real_semantic_rank(this); source_real >= 0 && target_real >= 0) {
		// Real assignment likewise permits the selected destination to lose
		// range or precision. make_implicit_cast owns the optional range
		// check after this relation has admitted the store.
		return implicit_conversion(static_cast<unsigned>(std::abs(target_real - source_real)));
	}
	return std::nullopt;
}

bool IntrinsicType::predefined_explicit_conversion_from(const Type* source) const {
	if (Type::predefined_explicit_conversion_from(source)) {
		return true;
	} else if (predefined_ordinal_type(this) && predefined_ordinal_type(source)) {
		// Explicit ordinal conversion is one direct width/sign operation. It is
		// intentionally broader than nominal enum/subrange compatibility but
		// does not make any such pair implicitly viable.
		return true;
	} else if (real_semantic_rank(this) >= 0 && real_semantic_rank(source) >= 0) {
		// Type(value) explicitly selects the destination representation, so
		// both real-family directions are one predefined cast even though
		// only widening is an implicit overload edge.
		return true;
	}
	// Only the address-sized integer types are direct pointer destinations.
	// Requiring an explicit nested cast for another integer width keeps the
	// address boundary visible and avoids a hidden pointer -> integer ->
	// integer chain.
	return (this == ptrint_type() || this == ptruint_type()) && (dynamic_cast<const PointerType*>(source) || predefined_object_reference_type(source) || source == ansistring_type());
}

std::optional<ValueConversion> ShortStringType::value_conversion_from(const Type* source) const {
	auto string = dynamic_cast<const ShortStringType*>(source);
	if (!string) {
		return std::nullopt;
	}
	unsigned distance = static_cast<unsigned>(std::abs(static_cast<int>(capacity) - static_cast<int>(string->capacity)));
	if (capacity == string->capacity) {
		return direct_conversion();
	} else if (string->capacity > capacity) {
		// A shorter destination cannot represent every source value, so this
		// direction requires explicit syntax rather than an implicit edge.
		return std::nullopt;
	} else {
		return implicit_conversion(distance);
	}
}

std::optional<ValueConversion> ShortStringType::destination_conversion_from(const Type* source) const {
	if (auto ordinary = value_conversion_from(source)) {
		return ordinary;
	} else if (source == ansistring_type()) {
		// AnsiString has no static capacity bound. Assignment to String[N]
		// remains legal and is classified as Narrowing; the runtime copies at
		// most this destination's declared capacity.
		return implicit_conversion(256 - capacity);
	} else if (auto string = dynamic_cast<const ShortStringType*>(source)) {
		// A known ShortString destination truncates excess payload according
		// to its declared capacity. The complete assignment query exposes this
		// direction as Narrowing.
		return implicit_conversion(static_cast<unsigned>(string->capacity - capacity));
	}
	return std::nullopt;
}

bool ShortStringType::predefined_explicit_conversion_from(const Type* source) const {
	// An explicit ShortString(AnsiString) construction copies the managed
	// string payload into this destination's fixed inline capacity. It may
	// truncate, so it must not become an implicit value-conversion edge used
	// by overload resolution.
	return Type::predefined_explicit_conversion_from(source) || source == ansistring_type();
}

std::optional<ValueConversion> FixedSetType::value_conversion_from(const Type* source) const {
	auto set = dynamic_cast<const FixedSetType*>(source);
	if (!set || !set->is_subtype_of(this)) {
		return std::nullopt;
	}
	// Set is the powerset constructor. Widening its ordinal domain preserves
	// every member and therefore needs no element-wise value conversion.
	// The emitter may still copy between distinct target-specific carriers.
	//
	// Preserve the element-domain widening distance for overload ranking:
	// an anonymous `set of Byte` constant is a direct match for both a
	// separately declared `set of Byte` and `set of Word`, but the former
	// must win. This remains one set conversion; it does not recursively
	// materialize or apply an element conversion.
	if (item_type == set->item_type) {
		return direct_conversion();
	}
	auto item_conversion = item_type->value_conversion_from(set->item_type);
	return implicit_conversion(item_conversion ? item_conversion->distance : 0);
}

bool FixedSetType::predefined_explicit_conversion_from(const Type* source) const {
	// An explicit set cast preserves stored ordinal membership keys even when
	// the item domains are not in the implicit subset relation.
	return Type::predefined_explicit_conversion_from(source) || dynamic_cast<const FixedSetType*>(source);
}

static bool interface_is_or_extends(const InterfaceType* source, const InterfaceType* target) {
	if (source == target) {
		return true;
	}
	for (InterfaceType* parent : source->super_interfaces) {
		if (interface_is_or_extends(parent, target)) {
			return true;
		}
	}
	return false;
}

bool InterfaceType::is_subtype_of(const Type* target) const {
	auto target_interface = dynamic_cast<const InterfaceType*>(target);
	return target_interface && interface_is_or_extends(this, target_interface);
}

static bool class_implements_interface(const ClassType* source, const InterfaceType* target) {
	for (const ClassType* current = source; current; current = current->super) {
		for (InterfaceType* implemented : current->implemented_interfaces) {
			if (interface_is_or_extends(implemented, target)) {
				return true;
			}
		}
	}
	return false;
}

bool ClassType::is_subtype_of(const Type* target) const {
	if (auto target_class = dynamic_cast<const ClassType*>(target)) {
		for (const ClassType* current = this; current; current = current->super) {
			if (current == target_class) {
				return true;
			}
		}
		return false;
	} else if (auto target_interface = dynamic_cast<const InterfaceType*>(target)) {
		return class_implements_interface(this, target_interface);
	}
	return false;
}

bool ObjectType::is_subtype_of(const Type* target) const {
	auto target_object = dynamic_cast<const ObjectType*>(target);
	if (!target_object) {
		return false;
	}
	for (const ObjectType* current = this; current; current = current->super) {
		if (current == target_object) {
			return true;
		}
	}
	return false;
}

static unsigned class_inheritance_distance(const ClassType* source, const ClassType* target) {
	unsigned distance = 0;
	for (const ClassType* current = source; current; current = current->super, ++distance) {
		if (current == target) {
			return distance;
		}
	}
	return std::numeric_limits<unsigned>::max();
}

static unsigned object_inheritance_distance(const ObjectType* source, const ObjectType* target) {
	unsigned distance = 0;
	for (const ObjectType* current = source; current; current = current->super, ++distance) {
		if (current == target) {
			return distance;
		}
	}
	return std::numeric_limits<unsigned>::max();
}

std::optional<ValueConversion> ClassType::value_conversion_from(const Type* source) const {
	auto source_class = dynamic_cast<const ClassType*>(source);
	if (!source_class || !source_class->is_subtype_of(this)) {
		return std::nullopt;
	}
	return implicit_conversion(class_inheritance_distance(source_class, this));
}

bool ClassType::predefined_explicit_conversion_from(const Type* source) const {
	if (Type::predefined_explicit_conversion_from(source)) {
		return true;
	} else if (auto source_class = dynamic_cast<const ClassType*>(source)) {
		return is_subtype_of(source_class) || source_class->is_subtype_of(this);
	} else if (auto source_interface = dynamic_cast<const InterfaceType*>(source)) {
		// A static interface -> class downcast has a defined adjustment only
		// when this destination class implements that exact interface family.
		return is_subtype_of(source_interface);
	}
	// Raw-pointer recovery is unchecked. The program must satisfy the same
	// live-object, alignment, and provenance preconditions as the emitted C++
	// pointer cast; TPCC deliberately does not try to prove them.
	return dynamic_cast<const PointerType*>(source) != nullptr;
}

std::optional<ValueConversion> InterfaceType::value_conversion_from(const Type* source) const {
	if (!source || !source->is_subtype_of(this)) {
		return std::nullopt;
	}
	return implicit_conversion(1);
}

bool InterfaceType::predefined_explicit_conversion_from(const Type* source) const {
	if (Type::predefined_explicit_conversion_from(source)) {
		return true;
	} else if (auto source_interface = dynamic_cast<const InterfaceType*>(source)) {
		return is_subtype_of(source_interface) || source_interface->is_subtype_of(this);
	}
	// An unrelated class/interface cross-cast is the checked `as` operation,
	// not an unchecked predefined T(E) conversion. A raw pointer is different:
	// it explicitly opts into the C++ object-model precondition.
	return dynamic_cast<const PointerType*>(source) != nullptr;
}

std::optional<ValueConversion> ObjectType::value_conversion_from(const Type* source) const {
	auto source_object = dynamic_cast<const ObjectType*>(source);
	if (!source_object || !source_object->is_subtype_of(this)) {
		return std::nullopt;
	}
	return implicit_conversion(object_inheritance_distance(source_object, this));
}

std::optional<ValueConversion> ClassRefType::value_conversion_from(const Type* source) const {
	auto source_ref = dynamic_cast<const ClassRefType*>(source);
	if (!source_ref) {
		return std::nullopt;
	}
	if (source_ref->target == target) {
		return direct_conversion();
	} else if (source_ref->target && source_ref->target->is_subtype_of(target)) {
		auto source_class = dynamic_cast<const ClassType*>(source_ref->target);
		auto target_class = dynamic_cast<const ClassType*>(target);
		unsigned distance = source_class && target_class ? class_inheritance_distance(source_class, target_class) : 1;
		return implicit_conversion(distance);
	}
	return std::nullopt;
}

bool ClassRefType::predefined_explicit_conversion_from(const Type* source) const {
	if (Type::predefined_explicit_conversion_from(source)) {
		return true;
	}
	auto source_ref = dynamic_cast<const ClassRefType*>(source);
	return source_ref && target && source_ref->target && (target->is_subtype_of(source_ref->target) || source_ref->target->is_subtype_of(target));
}

std::optional<ValueConversion> PointerType::value_conversion_from(const Type* source) const {
	auto source_pointer = dynamic_cast<const PointerType*>(source);
	if (source_pointer) {
		if (source_pointer->item_type == item_type) {
			return direct_conversion();
		} else if (source_pointer->is_untyped() || is_untyped()) {
			return implicit_conversion(20);
		} else {
			auto source_object = dynamic_cast<const ObjectType*>(source_pointer->item_type);
			auto target_object = dynamic_cast<const ObjectType*>(item_type);
			if (source_object && target_object && source_object->is_subtype_of(target_object)) {
				return implicit_conversion(object_inheritance_distance(source_object, target_object));
			}
			return std::nullopt;
		}
	} else if (is_untyped() && (dynamic_cast<const ClassType*>(source) || dynamic_cast<const ClassRefType*>(source))) {
		// A Pascal class instance and metaclass are already pointer-valued
		// references, so an API which explicitly asks for predefined untyped
		// Pointer may retain that opaque reference without a runtime
		// operation. Do not extend this to old-style object values,
		// interfaces, or ^T: those are different semantic/storage contracts.
		//
		// Keep this at the worst existing conversion distance so every
		// related class or class-reference overload wins independently of
		// inheritance depth. The ordinary matcher consults this destination
		// directly, rejects var/out before value conversion, and disables
		// value conversion while matching a declared conversion's source formal;
		// consequently this edge cannot become class -> Pointer -> T.
		return implicit_conversion(std::numeric_limits<unsigned>::max());
	}
	return std::nullopt;
}

bool PointerType::predefined_explicit_conversion_from(const Type* source) const {
	if (Type::predefined_explicit_conversion_from(source)) {
		return true;
	}
	// These are representation crossings, not subtyping or implicit
	// conversions. The result is useful only when the source address denotes
	// a suitably aligned live C++ object of the pointee type; no local check
	// can reconstruct that provenance.
	return dynamic_cast<const PointerType*>(source) || predefined_integer_family_type(source) || predefined_object_reference_type(source) || source == ansistring_type();
}

struct FoldedOrdinalValue {
	bool negative;
	uint64_t magnitude;
};

static std::optional<FoldedOrdinalValue> fold_ordinal_value(Node* node) {
	if (!node) {
		return std::nullopt;
	}
	ConstEvalContext ctx;
	ConstEvalResult folded = node->const_eval(ctx);
	if (folded.kind != ConstEvalResult::Kind::Success) {
		return std::nullopt;
	}
	if (auto integer = dynamic_cast<Integer*>(folded.node)) {
		return FoldedOrdinalValue{integer->negative, integer->value};
	} else if (auto member = dynamic_cast<EnumMemberRef*>(folded.node)) {
		if (member->value < 0) {
			return FoldedOrdinalValue{true, static_cast<uint64_t>(-(member->value + 1)) + 1};
		}
		return FoldedOrdinalValue{false, static_cast<uint64_t>(member->value)};
	}
	return std::nullopt;
}

static int compare_folded_ordinals(const FoldedOrdinalValue& a, const FoldedOrdinalValue& b) {
	if (a.negative != b.negative) {
		return a.negative ? -1 : 1;
	}
	if (a.magnitude == b.magnitude) {
		return 0;
	}
	if (a.negative) {
		return a.magnitude > b.magnitude ? -1 : 1;
	}
	return a.magnitude < b.magnitude ? -1 : 1;
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

static std::optional<OrdinalDomain> ordinal_domain(const Type* type) {
	if (!type) {
		return std::nullopt;
	}
	type = distinct_storage_type(type);
	if (auto range = dynamic_cast<const SubrangeType*>(type)) {
		auto lower = fold_ordinal_value(range->lower_bound);
		auto upper = fold_ordinal_value(range->upper_bound);
		if (!lower || !upper) {
			return std::nullopt;
		}
		if (dynamic_cast<const EnumType*>(range->base_type)) {
			return OrdinalDomain{OrdinalDomainFamily::Enumeration, range->base_type, *lower, *upper};
		}
		if (range->base_type == char_type()) {
			return OrdinalDomain{OrdinalDomainFamily::Character, char_type(), *lower, *upper};
		}
		OrdinalBounds bounds;
		if (integer_bounds(range->base_type, &bounds)) {
			return OrdinalDomain{OrdinalDomainFamily::Integer, nullptr, *lower, *upper};
		}
		return std::nullopt;
	} else if (auto intrinsic = dynamic_cast<const IntrinsicType*>(type); intrinsic && intrinsic->rank && intrinsic->ordinal_bounds) {
		const OrdinalBounds& bounds = *intrinsic->ordinal_bounds;
		return OrdinalDomain{OrdinalDomainFamily::Integer, nullptr, FoldedOrdinalValue{bounds.signed_type, bounds.signed_type ? bounds.min_magnitude : 0}, FoldedOrdinalValue{false, bounds.max_positive}};
	} else if (type == char_type()) {
		OrdinalBounds bounds;
		if (!intrinsic_ordinal_bounds(char_type(), &bounds)) {
			return std::nullopt;
		}
		return OrdinalDomain{OrdinalDomainFamily::Character, char_type(), FoldedOrdinalValue{false, 0}, FoldedOrdinalValue{false, bounds.max_positive}};
	} else if (auto enumeration = dynamic_cast<const EnumType*>(type)) {
		const auto* lower = enumeration->min_member();
		const auto* upper = enumeration->max_member();
		if (!lower || !upper) {
			return std::nullopt;
		}
		auto as_folded = [](int64_t value) {
			if (value < 0) {
				return FoldedOrdinalValue{true, static_cast<uint64_t>(-(value + 1)) + 1};
			}
			return FoldedOrdinalValue{false, static_cast<uint64_t>(value)};
		};
		return OrdinalDomain{OrdinalDomainFamily::Enumeration, type, as_folded(lower->value), as_folded(upper->value)};
	}
	return std::nullopt;
}

static bool ordinal_domain_is_subset(const Type* source, const Type* target) {
	auto source_domain = ordinal_domain(source);
	auto target_domain = ordinal_domain(target);
	if (!source_domain || !target_domain || source_domain->family != target_domain->family) {
		return false;
	}
	if (source_domain->family != OrdinalDomainFamily::Integer && source_domain->nominal_root != target_domain->nominal_root) {
		return false;
	}
	return compare_folded_ordinals(target_domain->lower, source_domain->lower) <= 0 && compare_folded_ordinals(target_domain->upper, source_domain->upper) >= 0;
}

static bool ordinal_domains_are_compatible(const Type* a, const Type* b) {
	auto a_domain = ordinal_domain(a);
	auto b_domain = ordinal_domain(b);
	if (!a_domain || !b_domain || a_domain->family != b_domain->family) {
		return false;
	}
	return a_domain->family == OrdinalDomainFamily::Integer || a_domain->nominal_root == b_domain->nominal_root;
}
} // namespace

bool IntrinsicType::is_subtype_of(const Type* target) const {
	if (this == target) {
		return true;
	}
	// Only predefined integer-family types use range subtyping. Char has an
	// ordinal range too, but it remains a distinct nominal ordinal family.
	return rank && ordinal_domain_is_subset(this, target);
}

std::optional<ValueConversion> EnumType::value_conversion_from(const Type* source) const {
	if (source && ordinal_domains_are_compatible(source, this) && source->is_subtype_of(this)) {
		// Only this enum's own subranges are compatible, and their complete
		// domains are contained by the enum declaration.
		return direct_conversion();
	}
	return std::nullopt;
}

bool EnumType::predefined_explicit_conversion_from(const Type* source) const {
	return Type::predefined_explicit_conversion_from(source) || predefined_ordinal_type(source);
}

bool IntrinsicType::same_cxx_carrier_definition_as(const Type* other) const {
	auto intrinsic = dynamic_cast<const IntrinsicType*>(other);
	return intrinsic && carrier && intrinsic->carrier && carrier == intrinsic->carrier;
}

bool SubrangeType::is_subtype_of(const Type* target) const {
	return this == target || ordinal_domain_is_subset(this, target);
}

bool SubrangeType::same_formal_contract_as(const Type* other) const {
	while (auto range = dynamic_cast<const SubrangeType*>(other)) {
		other = range->base_type;
	}
	return base_type->same_formal_contract_as(other);
}

bool FixedSetType::is_subtype_of(const Type* target) const {
	if (this == target) {
		return true;
	}
	auto set = dynamic_cast<const FixedSetType*>(target);
	return set && item_type->is_subtype_of(set->item_type);
}

std::optional<ValueConversion> SubrangeType::value_conversion_from(const Type* source) const {
	if (!source || !ordinal_domains_are_compatible(source, this)) {
		return std::nullopt;
	}
	// Contextual literals are checked against the exact endpoints before this
	// type-level path. This is the non-narrowing portion of the relation; the
	// complete assignment query obtains the reverse direction below.
	if (source->is_subtype_of(this)) {
		return direct_conversion();
	}
	return std::nullopt;
}

std::optional<ValueConversion> SubrangeType::destination_conversion_from(const Type* source) const {
	if (auto ordinary = value_conversion_from(source)) {
		return ordinary;
	}
	if (!source || !ordinal_domains_are_compatible(source, this)) {
		return std::nullopt;
	}
	// Assignment may store a wider value and let {$R+} enforce this
	// subrange's endpoints. The complete assignment query exposes this as
	// Narrowing without manufacturing a reverse subtype.
	return implicit_conversion();
}

bool SubrangeType::predefined_explicit_conversion_from(const Type* source) const {
	return Type::predefined_explicit_conversion_from(source) || predefined_ordinal_type(source);
}

std::optional<ValueConversion> RoutineType::value_conversion_from(const Type* source) const {
	auto routine = dynamic_cast<const RoutineType*>(source);
	if (routine && accepts_routine_value_from(routine)) {
		return direct_conversion();
	}
	return std::nullopt;
}

bool RoutineType::same_parameter_and_result_types_as(const RoutineType* other) const {
	if (!other || return_type != other->return_type || formals.size() != other->formals.size()) {
		return false;
	}
	auto overload_identity_type = [](const Type* type) {
		while (auto range = dynamic_cast<const SubrangeType*>(type)) {
			type = range->base_type;
		}
		return type;
	};
	for (size_t i = 0; i < formals.size(); ++i) {
		const Type* left = overload_identity_type(formals[i].ty);
		const Type* right = overload_identity_type(other->formals[i].ty);
		if (formals[i].mode != other->formals[i].mode || !left->same_formal_contract_as(right)) {
			return false;
		}
	}
	return true;
}

bool RoutineType::same_signature_as(const RoutineType* other) const {
	return other && kind == other->kind && same_parameter_and_result_types_as(other);
}

bool RoutineType::same_overload_signature_as(const RoutineType* other) const {
	if (!other || formals.size() != other->formals.size()) {
		return false;
	}
	auto overload_identity_type = [](const Type* type) {
		while (auto range = dynamic_cast<const SubrangeType*>(type)) {
			type = range->base_type;
		}
		return type;
	};
	for (size_t i = 0; i < formals.size(); ++i) {
		const Type* left = overload_identity_type(formals[i].ty);
		const Type* right = overload_identity_type(other->formals[i].ty);
		if (!left->same_formal_contract_as(right)) {
			return false;
		}
	}
	return true;
}

static RoutineKind routine_value_kind(RoutineKind kind) {
	// A class method is declared on m_meta, but a bound reference carries
	// that metaclass receiver in the same two-word representation as every
	// other `procedure of object`.
	return kind == CLASS_METHOD ? METHOD : kind;
}

bool RoutineType::accepts_routine_value_from(const RoutineType* source) const {
	if (!source) {
		return false;
	}
	RoutineKind source_kind = routine_value_kind(source->kind);
	RoutineKind target_kind = routine_value_kind(kind);
	if (source_kind != target_kind || (target_kind != ROUTINE && target_kind != METHOD)) {
		return false;
	}
	return same_parameter_and_result_types_as(source);
}

static bool routine_data_pointer_parameter_type(const Type* type) {
	while (auto incomplete = dynamic_cast<const IncompleteType*>(type)) {
		if (!incomplete->resolved) {
			return false;
		}
		type = incomplete->resolved;
	}
	// These are exactly the Pascal types emitted as ordinary C++ data
	// pointers. Do not use is_reference_type() here: a future managed or
	// capability reference may be nullable without sharing this ABI.
	return dynamic_cast<const PointerType*>(type) || dynamic_cast<const ClassType*>(type) || dynamic_cast<const InterfaceType*>(type) || dynamic_cast<const ClassRefType*>(type);
}

bool RoutineType::accepts_explicit_routine_cast_from(const RoutineType* source) const {
	if (!source) {
		return false;
	}
	RoutineKind source_kind = routine_value_kind(source->kind);
	RoutineKind target_kind = routine_value_kind(kind);
	if (source_kind != target_kind || (target_kind != ROUTINE && target_kind != METHOD) || return_type != source->return_type || formals.size() != source->formals.size()) {
		return false;
	}
	for (size_t i = 0; i < formals.size(); ++i) {
		const Parameter& target = formals[i];
		const Parameter& from = source->formals[i];
		if (target.mode != from.mode) {
			return false;
		}
		if (target.ty->same_formal_contract_as(from.ty)) {
			continue;
		}
		// The GNOME/GObject callback convention relied upon by TPCC covers
		// data pointers passed by value. A Pascal const/var/out parameter is a
		// C++ reference to the pointer object and has additional aliasing and
		// write-back obligations, so it deliberately remains exact.
		if (target.mode != ParamMode::Value || !routine_data_pointer_parameter_type(target.ty) || !routine_data_pointer_parameter_type(from.ty)) {
			return false;
		}
	}
	return true;
}

bool RoutineType::same_cxx_parameter_list_as(const RoutineType* other) const {
	if (!other || formals.size() != other->formals.size()) {
		return false;
	}
	auto carrier_mode = [](ParamMode mode) {
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
		auto a_open = dynamic_cast<OpenArrayType*>(a.ty);
		auto b_open = dynamic_cast<OpenArrayType*>(b.ty);
		if (a_open || b_open) {
			if (!a_open || !b_open || (a.mode == ParamMode::Const) != (b.mode == ParamMode::Const) || !a_open->item_type->same_cxx_carrier_as(b_open->item_type)) {
				return false;
			}
			continue;
		}
		if (carrier_mode(a.mode) != carrier_mode(b.mode)) {
			return false;
		}
		if (a.ty == unknown_type() || b.ty == unknown_type()) {
			// Omitted-type const and mutable formals lower to two explicit
			// storage-view carriers rather than to an arbitrary T or T&.
			if (a.ty != b.ty) {
				return false;
			}
			continue;
		}
		if (!a.ty->same_cxx_carrier_as(b.ty)) {
			return false;
		}
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
		if (ch == '\'') {
			r.push_back('\'');
		}
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

void ShortStringType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
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
	if (!frame) {
		return;
	}
	for (const auto& item : frame->value_declarations()) {
		ctx->add_type_edge(item.second.ty);
	}
}

void Type::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "truncated: yes";
}

const char* IncompleteType::diagnostic_kind() const {
	return "incomplete";
}

void IncompleteType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(resolved);
}

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

const char* DistinctType::diagnostic_kind() const {
	return "distinct_type";
}

void DistinctType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(base_type);
}

void DistinctType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "base: " << ctx->known_type_ref(base_type);
}

void DistinctType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "base: ...";
}

const char* FixedArrayType::diagnostic_kind() const {
	return "array";
}

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

const char* DynamicArrayType::diagnostic_kind() const {
	return "dynamic array";
}

void DynamicArrayType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(item_type);
}

void DynamicArrayType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "item: " << ctx->known_type_ref(item_type);
}

const char* OpenArrayType::diagnostic_kind() const {
	return "open array";
}

void OpenArrayType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(item_type);
}

void OpenArrayType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "item: " << ctx->known_type_ref(item_type);
}

const char* FixedSetType::diagnostic_kind() const {
	return "set";
}

void FixedSetType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(item_type);
}

void FixedSetType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "item: " << ctx->known_type_ref(item_type);
}

const char* TypedFileType::diagnostic_kind() const {
	return "file";
}

void TypedFileType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(item_type);
}

void TypedFileType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "item: " << ctx->known_type_ref(item_type);
}

const char* EnumType::diagnostic_kind() const {
	return "enum";
}

void EnumType::collect_diagnostic_edges(ErrorLetContext*) const {
}

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

static void collect_variant_diagnostic_edges(ErrorLetContext* ctx, const VariantPart* variant) {
	if (!variant) {
		return;
	}
	if (variant->selector_type) {
		ctx->add_type_edge(variant->selector_type);
	}
	for (const auto& arm : variant->arms) {
		for (const auto& field : arm.fields) {
			ctx->add_type_edge(field.ty);
		}
		collect_variant_diagnostic_edges(ctx, arm.variant);
	}
}

static void print_variant_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent, const VariantPart* variant) {
	if (!variant) {
		return;
	}
	ctx->indent(out, indent);
	out << "case ";
	if (variant->has_selector) {
		out << variant->selector_name << ": ";
	}
	out << ctx->known_type_ref(variant->selector_type) << " of\n";
	for (size_t i = 0; i < variant->arms.size(); ++i) {
		ctx->indent(out, indent + 1);
		out << "arm " << (i + 1) << ":\n";
		for (const auto& field : variant->arms[i].fields) {
			ctx->indent(out, indent + 2);
			out << (field.pas_name.empty() ? "<field>" : field.pas_name) << ": " << ctx->known_type_ref(field.ty) << ";\n";
		}
		print_variant_diagnostic_definition(ctx, out, indent + 2, variant->arms[i].variant);
	}
}

const char* RecordType::diagnostic_kind() const {
	return "record";
}

void RecordType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
	collect_variant_diagnostic_edges(ctx, variant);
}

void RecordType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->print_frame_members(out, children, indent + 1);
	print_variant_diagnostic_definition(ctx, out, indent + 1, variant);
	ctx->indent(out, indent);
	out << "end";
}

void RecordType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const {
	out << " ... end";
}

const char* PackedRecordType::diagnostic_kind() const {
	return "packed record";
}

void PackedRecordType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
	collect_variant_diagnostic_edges(ctx, variant);
}

void PackedRecordType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->print_frame_members(out, children, indent + 1);
	print_variant_diagnostic_definition(ctx, out, indent + 1, variant);
	ctx->indent(out, indent);
	out << "end";
}

void PackedRecordType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const {
	out << " ... end";
}

const char* InterfaceType::diagnostic_kind() const {
	return "interface";
}

void InterfaceType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::AggregateMembers);
	add_frame_value_type_edges(ctx, children);
	for (auto* i : super_interfaces) {
		ctx->add_type_edge(i);
	}
}

void InterfaceType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	if (!super_interfaces.empty()) {
		ctx->indent(out, indent + 1);
		out << "inherits:";
		for (auto* i : super_interfaces) {
			out << " " << ctx->known_type_ref(i);
		}
		out << "\n";
	}
	ctx->print_frame_members(out, children, indent + 1);
	ctx->indent(out, indent);
	out << "end";
}

void InterfaceType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const {
	out << " ... end";
}

const char* ClassType::diagnostic_kind() const {
	return "class";
}

void ClassType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(super);
	for (auto* i : implemented_interfaces) {
		ctx->add_type_edge(i);
	}
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
		for (auto* i : implemented_interfaces) {
			out << " " << ctx->known_type_ref(i);
		}
		out << "\n";
	}
	if (class_constructor) {
		ctx->indent(out, indent + 1);
		out << "class constructor: " << ctx->known_value_ref(class_constructor) << "\n";
	}
	if (class_destructor) {
		ctx->indent(out, indent + 1);
		out << "class destructor: " << ctx->known_value_ref(class_destructor) << "\n";
	}
	ctx->print_frame_members(out, children, indent + 1);
	ctx->indent(out, indent);
	out << "end";
}

void ClassType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const {
	out << " ... end";
}

const char* ClassRefType::diagnostic_kind() const {
	return "classref";
}

void ClassRefType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(target);
}

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

const char* ObjectType::diagnostic_kind() const {
	return "object";
}

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

void ObjectType::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const {
	out << " ... end";
}

const char* PointerType::diagnostic_kind() const {
	return "pointer";
}

void PointerType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	if (item_type) {
		ctx->add_type_edge(item_type);
	}
}

void PointerType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	if (item_type) {
		out << "to: " << ctx->known_type_ref(item_type);
	} else {
		out << "untyped";
	}
}

void PointerType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << (item_type ? "to: ..." : "untyped");
}

const char* ModuleType::diagnostic_kind() const {
	return "module";
}

void ModuleType::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_frame_edge(children, DiagnosticFrameUse::ModuleMembers);
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

const char* UnitType::diagnostic_kind() const {
	return "unit";
}

void UnitType::collect_diagnostic_edges(ErrorLetContext*) const {
}

void UnitType::print_diagnostic_definition(ErrorLetContext*, std::ostringstream&, unsigned) const {
}

const char* UntypedIntegerType::diagnostic_kind() const {
	return "untyped_integer";
}

void UntypedIntegerType::collect_diagnostic_edges(ErrorLetContext*) const {
}

void UntypedIntegerType::print_diagnostic_definition(ErrorLetContext*, std::ostringstream&, unsigned) const {
}

const char* UntypedRealType::diagnostic_kind() const {
	return "untyped_real";
}

void UntypedRealType::collect_diagnostic_edges(ErrorLetContext*) const {
}

void UntypedRealType::print_diagnostic_definition(ErrorLetContext*, std::ostringstream&, unsigned) const {
}

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

const char* RoutineType::diagnostic_kind() const {
	return "routine";
}

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
	if (kind == CONSTRUCTOR) {
		out << "constructor";
	} else if (kind == CLASS_CONSTRUCTOR) {
		out << "class_constructor";
	} else if (kind == CLASS_DESTRUCTOR) {
		out << "class_destructor";
	} else if (kind == DESTRUCTOR) {
		out << "destructor";
	} else if (kind == METHOD) {
		out << "method";
	} else if (kind == ROUTINE) {
		out << "routine";
	} else {
		out << "class_method";
	}
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "signature: (";
	for (size_t i = 0; i < formals.size(); i++) {
		if (i) {
			out << "; ";
		}
		const auto& p = formals[i];
		out << param_mode_text(p.mode) << p.pas_name << ": " << ctx->known_type_ref(p.ty);
		if (p.default_value) {
			out << " = " << ctx->known_value_ref(p.default_value);
		}
	}
	out << ")";
	if (return_type) {
		out << ": " << ctx->known_type_ref(return_type);
	}
}

void RoutineType::print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "signature: ...";
}

SubrangeType::SubrangeType(SourceLocation source_location, std::string cxx_name, Type* base_type, Node* lower_bound, Node* upper_bound) : Type(std::move(source_location)), cxx_name(std::move(cxx_name)) {
	assert(!this->cxx_name.empty());
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
