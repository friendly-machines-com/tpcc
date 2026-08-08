#include "builtins.h"
#include "cst.h"
#include "evaluator.h"
#include "frame.h"
#include "operators.h"
#include "types.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <string>

IntrinsicType::IntrinsicType(SourceLocation source_location, std::string cxx_name, std::optional<int> rank, std::optional<OrdinalBounds> ordinal_bounds, std::optional<TypeLayout> layout, std::optional<IntrinsicCarrier> carrier) : Type(std::move(source_location)), cxx_name(std::move(cxx_name)), rank(std::move(rank)), ordinal_bounds(std::move(ordinal_bounds)), layout(std::move(layout)), carrier(std::move(carrier)) {
}

Builtin::Builtin(const BuiltinDesc* desc) : desc(desc) {
}

// Integer rows are ordered narrowest -> widest. IntrinsicType's destination
// conversion rule uses the rank and explicit bounds to score widening and
// narrowing without imposing one common type before overload selection.
namespace {
constexpr uint64_t unsigned_max_for_bits(unsigned bits) {
	return bits == 64 ? UINT64_MAX : ((uint64_t{1} << bits) - 1);
}

constexpr OrdinalBounds unsigned_bounds(unsigned bits) {
	return OrdinalBounds{false, 0, unsigned_max_for_bits(bits)};
}

constexpr OrdinalBounds signed_bounds(unsigned bits) {
	return OrdinalBounds{
	    true,
	    uint64_t{1} << (bits - 1),
	    unsigned_max_for_bits(bits - 1),
	};
}

IntrinsicType k_byte(SourceLocation::builtin(), "::u_system::t_byte", 0, unsigned_bounds(8), TypeLayout{1, 1}, IntrinsicCarrier::UInt8);
IntrinsicType k_shortint(SourceLocation::builtin(), "::u_system::t_shortint", 1, signed_bounds(8), TypeLayout{1, 1}, IntrinsicCarrier::Int8);
IntrinsicType k_word(SourceLocation::builtin(), "::u_system::t_word", 2, unsigned_bounds(16), TypeLayout{2, 2}, IntrinsicCarrier::UInt16);
IntrinsicType k_smallint(SourceLocation::builtin(), "::u_system::t_smallint", 3, signed_bounds(16), TypeLayout{2, 2}, IntrinsicCarrier::Int16);
IntrinsicType k_longword(SourceLocation::builtin(), "::u_system::t_longword", 4, unsigned_bounds(32), TypeLayout{4, 4}, IntrinsicCarrier::UInt32);
IntrinsicType k_integer(SourceLocation::builtin(), "::u_system::t_integer", 5, signed_bounds(32), TypeLayout{4, 4}, IntrinsicCarrier::Int32);
IntrinsicType k_longint(SourceLocation::builtin(), "::u_system::t_longint", 6, signed_bounds(32), TypeLayout{4, 4}, IntrinsicCarrier::Int32);
IntrinsicType k_qword(SourceLocation::builtin(), "::u_system::t_qword", 7, unsigned_bounds(64), TypeLayout{8, 8}, IntrinsicCarrier::UInt64);
IntrinsicType k_int64(SourceLocation::builtin(), "::u_system::t_int64", 8, signed_bounds(64), TypeLayout{8, 8}, IntrinsicCarrier::Int64);
IntrinsicType k_set(SourceLocation::builtin(), "::u_system::t_set", {});
IntrinsicType k_single(SourceLocation::builtin(), "::u_system::t_single", {}, {}, TypeLayout{4, 4}, IntrinsicCarrier::Float);
IntrinsicType k_double(SourceLocation::builtin(), "::u_system::t_double", {}, {}, TypeLayout{8, 8}, IntrinsicCarrier::Double);
IntrinsicType k_extended(SourceLocation::builtin(), "::u_system::t_extended", {}, {}, TypeLayout{16, 16}, IntrinsicCarrier::LongDouble);
EnumType k_boolean(SourceLocation::builtin(), "::u_system::t_boolean", "false", "true", 8, false);
IntrinsicType k_char(SourceLocation::builtin(), "::u_system::t_char", {}, unsigned_bounds(8), TypeLayout{1, 1}, IntrinsicCarrier::Character);
ShortStringType k_shortstring(SourceLocation::builtin(), 255);
IntrinsicType k_ansistring(SourceLocation::builtin(), "::u_system::t_ansistring", {}, {}, TypeLayout{8, 8}, IntrinsicCarrier::AnsiString);
IntrinsicType k_text(SourceLocation::builtin(), "::u_system::t_text", {}, {}, TypeLayout{8, 8}, IntrinsicCarrier::Text);
IntrinsicType k_file(SourceLocation::builtin(), "::u_system::t_file", {}, {}, TypeLayout{8, 8}, IntrinsicCarrier::File);
PointerType k_pointer(SourceLocation::builtin(), nullptr, "::u_system::t_pointer");
IntrinsicType k_fixedarray(SourceLocation::builtin(), "::u_system::t_fixedarray", {});
IntrinsicType k_unknown(SourceLocation::builtin(), "::u_system::tpcc_unknown_type", {});

struct TMethodDefinition {
	Frame children;
	RecordType type;
	StorageSlot code;
	StorageSlot data;

	TMethodDefinition() : children(nullptr), type(SourceLocation::builtin(), &children), code("p_code", &k_pointer), data("p_data", &k_pointer) {
		type.cxx_name = "::u_system::t_tmethod";
		children.register_variable("code", &code, &k_pointer);
		children.register_variable("data", &data, &k_pointer);
		type.fields.push_back(AggregateField{"code", &code, &k_pointer});
		type.fields.push_back(AggregateField{"data", &data, &k_pointer});
	}
};

TMethodDefinition& tmethod_definition() {
	static TMethodDefinition definition;
	return definition;
}

Type* const k_all_intrinsics[] = {
    &k_byte, &k_shortint, &k_word, &k_smallint, &k_longword, &k_integer, &k_longint, &k_qword, &k_int64, &k_set, &k_single, &k_double, &k_extended, &k_boolean, &k_char, &k_shortstring, &k_ansistring, &k_text, &k_file, &k_pointer, &k_fixedarray, &k_unknown,
    //    &k_m_iobject,
};
} // namespace

UnitType& unit_type() {
	static UnitType t(SourceLocation::internal());
	return t;
}

UntypedIntegerType& untyped_integer_type() {
	static UntypedIntegerType t(SourceLocation::internal());
	return t;
}

Type* byte_type() {
	return &k_byte;
}

Type* shortint_type() {
	return &k_shortint;
}

Type* word_type() {
	return &k_word;
}

Type* smallint_type() {
	return &k_smallint;
}

Type* cardinal_type() {
	return &k_longword;
}

Type* integer_type() {
	return &k_integer;
}

Type* longint_type() {
	return &k_longint;
}

Type* sizeint_type() {
	return int64_type();
}

Type* qword_type() {
	return &k_qword;
}

Type* int64_type() {
	return &k_int64;
}

Type* pointer_type() {
	return &k_pointer;
}

Type* ptrint_type() {
	return int64_type();
}

Type* ptruint_type() {
	return qword_type();
}

Type* boolean_type() {
	return &k_boolean;
}

Type* char_type() {
	return &k_char;
}

ShortStringType* shortstring_type(uint8_t capacity) {
	if (capacity == 255) {
		return &k_shortstring;
	}
	static std::array<ShortStringType*, 256> types{};
	ShortStringType*& result = types[capacity];
	if (!result) {
		result = new ShortStringType(SourceLocation::builtin(), capacity);
	}
	return result;
}

Type* ansistring_type() {
	return &k_ansistring;
}

Type* text_type() {
	return &k_text;
}

Type* file_type() {
	return &k_file;
}

Type* single_type() {
	return &k_single;
}

Type* double_type() {
	return &k_double;
}

Type* extended_type() {
	return &k_extended;
}

Type* set_type() {
	return &k_set;
}

Type* fixedarray_type() {
	return &k_fixedarray;
}

Type* unknown_type() {
	return &k_unknown;
}

RecordType* tmethod_type() {
	return &tmethod_definition().type;
}

StorageSlot* tmethod_code_field() {
	return &tmethod_definition().code;
}

StorageSlot* tmethod_data_field() {
	return &tmethod_definition().data;
}

bool intrinsic_ordinal_bounds(Type* ty, OrdinalBounds* out) {
	ty = distinct_storage_type(ty);
	auto intrinsic = dynamic_cast<const IntrinsicType*>(ty);
	if (!intrinsic || !intrinsic->ordinal_bounds) {
		return false;
	}
	*out = *intrinsic->ordinal_bounds;
	return true;
}

Type* IntrinsicType::sequence_element_type() const {
	return carrier == IntrinsicCarrier::AnsiString ? char_type() : nullptr;
}

Type* IntrinsicType::sequence_index_type() const {
	return carrier == IntrinsicCarrier::AnsiString ? integer_type() : nullptr;
}

Type* IntrinsicType::sequence_length_type() const {
	return carrier == IntrinsicCarrier::AnsiString ? sizeint_type() : nullptr;
}

bool IntrinsicType::sequence_is_resizable() const {
	return carrier == IntrinsicCarrier::AnsiString;
}

bool IntrinsicType::has_managed_lifetime() const {
	return carrier == IntrinsicCarrier::AnsiString;
}

bool integer_bounds(const Type* ty, OrdinalBounds* out) {
	ty = distinct_storage_type(ty);
	auto intrinsic = dynamic_cast<const IntrinsicType*>(ty);
	if (!intrinsic || !intrinsic->rank || !intrinsic->ordinal_bounds) {
		return false;
	}
	*out = *intrinsic->ordinal_bounds;
	return true;
}

static const Integer* const_integer_arg(Node* n) {
	return dynamic_cast<const Integer*>(n);
}

static bool const_numeric_as_long_double(Node* n, long double* out) {
	if (auto i = dynamic_cast<const Integer*>(n)) {
		*out = static_cast<long double>(i->value);
		if (i->negative) {
			*out = -*out;
		}
		return true;
	}
	if (auto r = dynamic_cast<const Real*>(n)) {
		*out = r->value;
		return true;
	}
	return false;
}

static ConstEvalResult fold_integer_result(uint64_t magnitude, bool negative, Type* ty) {
	return const_convert_integer(magnitude, negative, ty, ty);
}

static ConstEvalResult fold_implicit(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	// system.pp's predefined integer operator := declarations are pure
	// representation conversions. Explicit Type(constant) syntax selects the
	// same declaration, so its declaration-local RTL implementation must
	// retain the constant expression instead of turning it into a runtime
	// call. User conversion bodies do not carry this o_implicit descriptor.
	if (args.size() != 1 || !const_integer_arg(args[0])) {
		return ConstEvalResult::not_constant();
	}
	const Integer* value = const_integer_arg(args[0]);
	return fold_integer_result(value->value, value->negative, result_ty);
}

static uint64_t unchecked_integer_bits(const Integer* value) {
	return value->negative ? uint64_t{0} - value->value : value->value;
}

static ConstEvalResult fold_unchecked_integer_bits(uint64_t bits, Type* result_ty) {
	// The explicit ordinal cast is TPCC's existing representation conversion:
	// it truncates to the Pascal carrier width and then interprets that bit
	// pattern with the carrier's signedness.
	return const_explicit_ordinal_cast(bits, false, result_ty);
}

static ConstEvalResult fold_unary_minus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0])) {
		return ConstEvalResult::not_constant();
	}
	auto i = const_integer_arg(args[0]);
	return fold_integer_result(i->value, !i->negative && i->value != 0, result_ty);
}

static ConstEvalResult fold_unchecked_unary_minus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0])) {
		return ConstEvalResult::not_constant();
	}
	return fold_unchecked_integer_bits(uint64_t{0} - unchecked_integer_bits(const_integer_arg(args[0])), result_ty);
}

static ConstEvalResult fold_unary_plus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0])) {
		return ConstEvalResult::not_constant();
	}
	auto i = const_integer_arg(args[0]);
	return fold_integer_result(i->value, i->negative, result_ty);
}

struct ConstantOrdinalCarrier {
	unsigned bits;
	bool signed_type;
};

static std::optional<ConstantOrdinalCarrier> constant_ordinal_carrier(Type* type) {
	while (auto range = dynamic_cast<SubrangeType*>(type)) {
		type = range->base_type;
	}
	OrdinalBounds bounds;
	if (integer_bounds(type, &bounds)) {
		uint64_t high_bit = bounds.signed_type ? bounds.min_magnitude : bounds.max_positive;
		unsigned bits = 0;
		do {
			++bits;
			high_bit >>= 1;
		} while (high_bit != 0);
		return ConstantOrdinalCarrier{bits, bounds.signed_type};
	}
	if (type == char_type()) {
		return ConstantOrdinalCarrier{8, false};
	}
	if (auto enumeration = dynamic_cast<EnumType*>(type)) {
		return ConstantOrdinalCarrier{enumeration->carrier_bits, enumeration->carrier_signed};
	}
	return std::nullopt;
}

static std::optional<std::pair<bool, uint64_t>> constant_ordinal_value(Node* value) {
	if (auto integer = dynamic_cast<Integer*>(value)) {
		return std::pair{integer->negative, integer->value};
	}
	if (auto member = dynamic_cast<EnumMemberRef*>(value)) {
		const bool negative = member->value < 0;
		const uint64_t magnitude = negative ? static_cast<uint64_t>(-(member->value + 1)) + 1 : static_cast<uint64_t>(member->value);
		return std::pair{negative, magnitude};
	}
	if (auto character = dynamic_cast<String*>(value); character && character->ty == char_type() && character->value.size() == 1) {
		return std::pair{false, static_cast<uint64_t>(static_cast<unsigned char>(character->value.front()))};
	}
	return std::nullopt;
}

static uint64_t constant_ordinal_mask(unsigned bits) {
	return bits == 64 ? UINT64_MAX : (uint64_t{1} << bits) - 1;
}

static uint64_t constant_ordinal_bits(bool negative, uint64_t magnitude, unsigned bits) {
	const uint64_t raw = negative ? uint64_t{0} - magnitude : magnitude;
	return raw & constant_ordinal_mask(bits);
}

static ConstEvalResult fold_ord(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !args[0]) {
		return ConstEvalResult::not_constant();
	}
	auto value = constant_ordinal_value(args[0]);
	if (!value) {
		return ConstEvalResult::not_constant();
	}
	// The RTL returns Ord through its declared Cardinal result, including the
	// two's-complement representation of negative enumeration values. Use the
	// same ordinary ordinal cast here so a constant call has exactly that
	// result rather than acquiring separate constant-only semantics.
	return const_explicit_ordinal_cast(value->second, value->first, result_ty);
}

enum class ComparisonKind {
	LessThan,
	LessThanOrEqual,
	Equal,
	GreaterThan,
	GreaterThanOrEqual,
};

static int compare_ordinal_constants(const std::pair<bool, uint64_t>& a, const std::pair<bool, uint64_t>& b) {
	if (a.first != b.first) {
		return a.first ? -1 : 1;
	}
	if (a.first) {
		// Both negative: larger magnitude is the smaller value.
		if (a.second != b.second) {
			return a.second > b.second ? -1 : 1;
		}
		return 0;
	}
	if (a.second != b.second) {
		return a.second < b.second ? -1 : 1;
	}
	return 0;
}

static ConstEvalResult fold_comparison(ComparisonKind kind, const std::vector<Node*>& args) {
	if (args.size() != 2 || !args[0] || !args[1]) {
		return ConstEvalResult::not_constant();
	}
	auto a = constant_ordinal_value(args[0]);
	auto b = constant_ordinal_value(args[1]);
	if (!a || !b) {
		return ConstEvalResult::not_constant();
	}
	const int c = compare_ordinal_constants(*a, *b);
	bool result;
	switch (kind) {
	case ComparisonKind::LessThan:
		result = c < 0;
		break;
	case ComparisonKind::LessThanOrEqual:
		result = c <= 0;
		break;
	case ComparisonKind::Equal:
		result = c == 0;
		break;
	case ComparisonKind::GreaterThan:
		result = c > 0;
		break;
	case ComparisonKind::GreaterThanOrEqual:
		result = c >= 0;
		break;
	}
	return ConstEvalResult::success(new EnumMemberRef(result ? "::u_system::t_boolean::p_true" : "::u_system::t_boolean::p_false", result ? 1 : 0, boolean_type()));
}

static ConstEvalResult fold_lessthan(ConstEvalContext&, Type*, const std::vector<Node*>& args) {
	return fold_comparison(ComparisonKind::LessThan, args);
}

static ConstEvalResult fold_lessthanorequal(ConstEvalContext&, Type*, const std::vector<Node*>& args) {
	return fold_comparison(ComparisonKind::LessThanOrEqual, args);
}

static ConstEvalResult fold_equal(ConstEvalContext&, Type*, const std::vector<Node*>& args) {
	return fold_comparison(ComparisonKind::Equal, args);
}

static ConstEvalResult fold_greaterthan(ConstEvalContext&, Type*, const std::vector<Node*>& args) {
	return fold_comparison(ComparisonKind::GreaterThan, args);
}

static ConstEvalResult fold_greaterthanorequal(ConstEvalContext&, Type*, const std::vector<Node*>& args) {
	return fold_comparison(ComparisonKind::GreaterThanOrEqual, args);
}

static ConstEvalResult fold_assigned(ConstEvalContext&, Type*, const std::vector<Node*>& args) {
	if (args.size() != 1 || !args[0]) {
		return ConstEvalResult::not_constant();
	}
	if (dynamic_cast<NilLiteral*>(args[0])) {
		return ConstEvalResult::success(new EnumMemberRef("::u_system::t_boolean::p_false", 0, boolean_type()));
	}
	if (dynamic_cast<AddrOf*>(args[0]) || dynamic_cast<RoutineRef*>(args[0])) {
		return ConstEvalResult::success(new EnumMemberRef("::u_system::t_boolean::p_true", 1, boolean_type()));
	}
	return ConstEvalResult::not_constant();
}

static ConstEvalResult fold_shift(Type* result_ty, const std::vector<Node*>& args, bool left) {
	if (args.size() != 2 || !args[0] || !args[1]) {
		return ConstEvalResult::not_constant();
	}
	auto value = constant_ordinal_value(args[0]);
	auto count = constant_ordinal_value(args[1]);
	auto carrier = constant_ordinal_carrier(result_ty);
	if (!value || !count || !carrier || carrier->bits == 0) {
		return ConstEvalResult::not_constant();
	}

	const uint64_t mask = constant_ordinal_mask(carrier->bits);
	const uint64_t raw = constant_ordinal_bits(value->first, value->second, carrier->bits);
	const uint64_t raw_count = count->first ? uint64_t{0} - count->second : count->second;
	// System's runtime shift helpers mask the count at the promoted result
	// width, including negative counts. Mirror their unsigned operation here:
	// besides keeping constant and runtime evaluation identical, it avoids
	// C++'s undefined signed and oversized shifts.
	const unsigned amount = static_cast<unsigned>(raw_count & (carrier->bits - 1));
	const uint64_t shifted = left ? (raw << amount) & mask : raw >> amount;
	return const_explicit_ordinal_cast(shifted, false, result_ty);
}

static ConstEvalResult fold_leftshift(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_shift(result_ty, args, true);
}

static ConstEvalResult fold_rightshift(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_shift(result_ty, args, false);
}

using ConstantSetKey = std::pair<bool, uint64_t>;

struct ConstantSetRange {
	ConstantSetKey lower;
	ConstantSetKey upper;
};

static int compare_constant_set_keys(const ConstantSetKey& first, const ConstantSetKey& second) {
	if (first.first != second.first) {
		return first.first ? -1 : 1;
	}
	if (first.second == second.second) {
		return 0;
	}
	if (first.first) {
		return first.second > second.second ? -1 : 1;
	}
	return first.second < second.second ? -1 : 1;
}

static std::optional<ConstantSetKey> constant_set_predecessor(ConstantSetKey value) {
	if (value.first) {
		if (value.second == UINT64_MAX) {
			return std::nullopt;
		}
		++value.second;
		return value;
	}
	if (value.second != 0) {
		--value.second;
		return value;
	}
	return ConstantSetKey{true, 1};
}

static std::optional<ConstantSetKey> constant_set_successor(ConstantSetKey value) {
	if (value.first) {
		if (value.second > 1) {
			--value.second;
			return value;
		}
		return ConstantSetKey{false, 0};
	}
	if (value.second == UINT64_MAX) {
		return std::nullopt;
	}
	++value.second;
	return value;
}

static std::optional<std::vector<ConstantSetRange>> constant_set_ranges(SetLiteral* set) {
	if (!set) {
		return std::nullopt;
	}
	std::vector<ConstantSetRange> ranges;
	ranges.reserve(set->items.size());
	for (const SetLiteral::Item& item : set->items) {
		auto lower = constant_ordinal_value(item.lower);
		auto upper = constant_ordinal_value(item.upper ? item.upper : item.lower);
		if (!lower || !upper) {
			return std::nullopt;
		}
		ConstantSetRange range{*lower, *upper};
		if (compare_constant_set_keys(range.lower, range.upper) <= 0) {
			ranges.push_back(range);
		}
	}
	return ranges;
}

static ConstEvalResult constant_set_literal(Type* result_ty, const std::vector<ConstantSetRange>& ranges) {
	auto result_set = dynamic_cast<FixedSetType*>(result_ty);
	if (!result_set) {
		return ConstEvalResult::not_constant();
	}
	std::vector<SetLiteral::Item> items;
	items.reserve(ranges.size());
	for (const ConstantSetRange& range : ranges) {
		ConstEvalResult lower = const_explicit_ordinal_cast(range.lower.second, range.lower.first, result_set->item_type);
		if (lower.kind != ConstEvalResult::Kind::Success) {
			return lower;
		}
		Node* upper_node = nullptr;
		if (compare_constant_set_keys(range.lower, range.upper) != 0) {
			ConstEvalResult upper = const_explicit_ordinal_cast(range.upper.second, range.upper.first, result_set->item_type);
			if (upper.kind != ConstEvalResult::Kind::Success) {
				return upper;
			}
			upper_node = upper.node;
		}
		items.push_back(SetLiteral::Item{lower.node, upper_node});
	}
	return ConstEvalResult::success(new SetLiteral(std::move(items), result_ty));
}

static ConstEvalResult fold_set_union(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !dynamic_cast<FixedSetType*>(result_ty)) {
		return ConstEvalResult::not_constant();
	}
	auto first = dynamic_cast<SetLiteral*>(args[0]);
	auto second = dynamic_cast<SetLiteral*>(args[1]);
	if (!first || !second) {
		return ConstEvalResult::not_constant();
	}
	std::vector<SetLiteral::Item> items = first->items;
	items.insert(items.end(), second->items.begin(), second->items.end());
	return ConstEvalResult::success(new SetLiteral(std::move(items), result_ty));
}

static ConstEvalResult fold_set_difference(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !dynamic_cast<FixedSetType*>(result_ty)) {
		return ConstEvalResult::not_constant();
	}
	auto remaining = constant_set_ranges(dynamic_cast<SetLiteral*>(args[0]));
	auto removed = constant_set_ranges(dynamic_cast<SetLiteral*>(args[1]));
	if (!remaining || !removed) {
		return ConstEvalResult::not_constant();
	}

	for (const ConstantSetRange& removal : *removed) {
		std::vector<ConstantSetRange> next;
		next.reserve(remaining->size() + 1);
		for (const ConstantSetRange& range : *remaining) {
			if (compare_constant_set_keys(removal.upper, range.lower) < 0 || compare_constant_set_keys(range.upper, removal.lower) < 0) {
				next.push_back(range);
				continue;
			}
			if (compare_constant_set_keys(range.lower, removal.lower) < 0) {
				auto upper = constant_set_predecessor(removal.lower);
				if (!upper) {
					return ConstEvalResult::not_constant();
				}
				next.push_back(ConstantSetRange{range.lower, *upper});
			}
			if (compare_constant_set_keys(removal.upper, range.upper) < 0) {
				auto lower = constant_set_successor(removal.upper);
				if (!lower) {
					return ConstEvalResult::not_constant();
				}
				next.push_back(ConstantSetRange{*lower, range.upper});
			}
		}
		*remaining = std::move(next);
	}
	return constant_set_literal(result_ty, *remaining);
}

static ConstEvalResult fold_abs_impl(Type* result_ty, const std::vector<Node*>& args, bool checked) {
	if (args.size() != 1 || !args[0]) {
		return ConstEvalResult::not_constant();
	}
	if (auto real = dynamic_cast<Real*>(args[0])) {
		return ConstEvalResult::success(new Real(::fabsl(real->value), result_ty));
	}
	auto value = constant_ordinal_value(args[0]);
	auto carrier = constant_ordinal_carrier(result_ty);
	if (!value || !carrier) {
		return ConstEvalResult::not_constant();
	}
	const uint64_t raw = constant_ordinal_bits(value->first, value->second, carrier->bits);
	if (checked && value->first && carrier->signed_type && raw == (uint64_t{1} << (carrier->bits - 1))) {
		return ConstEvalResult::error("integer constant overflow");
	}
	const uint64_t absolute = value->first ? (uint64_t{0} - raw) & constant_ordinal_mask(carrier->bits) : raw;
	return const_explicit_ordinal_cast(absolute, false, result_ty);
}

static ConstEvalResult fold_abs(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_abs_impl(result_ty, args, true);
}

static ConstEvalResult fold_unchecked_abs(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_abs_impl(result_ty, args, false);
}

static ConstEvalResult fold_ordinal_step(Type* result_ty, const std::vector<Node*>& args, bool increment, bool checked) {
	if (args.size() != 1 || !args[0]) {
		return ConstEvalResult::not_constant();
	}
	auto value = constant_ordinal_value(args[0]);
	auto carrier = constant_ordinal_carrier(result_ty);
	if (!value || !carrier || carrier->bits == 0) {
		return ConstEvalResult::not_constant();
	}
	const uint64_t mask = constant_ordinal_mask(carrier->bits);
	const uint64_t raw = constant_ordinal_bits(value->first, value->second, carrier->bits);
	if (checked) {
		const uint64_t minimum = carrier->signed_type ? uint64_t{1} << (carrier->bits - 1) : 0;
		const uint64_t maximum = carrier->signed_type ? minimum - 1 : mask;
		if ((increment && raw == maximum) || (!increment && raw == minimum)) {
			return ConstEvalResult::error("integer constant overflow");
		}
	}
	const uint64_t stepped = increment ? (raw + 1) & mask : (raw - 1) & mask;
	return const_explicit_ordinal_cast(stepped, false, result_ty);
}

static ConstEvalResult fold_succ(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_ordinal_step(result_ty, args, true, true);
}

static ConstEvalResult fold_unchecked_succ(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_ordinal_step(result_ty, args, true, false);
}

static ConstEvalResult fold_pred(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_ordinal_step(result_ty, args, false, true);
}

static ConstEvalResult fold_unchecked_pred(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_ordinal_step(result_ty, args, false, false);
}

enum class BitwiseOperation {
	BitwiseOr,
	BitwiseAnd,
	BitwiseXor,
};

// FIXME: maybe get the target types from the operand types.
static ConstEvalResult fold_bitwise(Type* result_ty, const std::vector<Node*>& args, BitwiseOperation op) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1])) {
		return ConstEvalResult::not_constant();
	}
	auto a_arg = const_integer_arg(args[0]);
	auto a = unchecked_integer_bits(a_arg);
	auto b_arg = const_integer_arg(args[1]);
	auto b = unchecked_integer_bits(b_arg);

	uint64_t mag = a;
	switch (op) {
	case BitwiseOperation::BitwiseOr:
		mag |= b;
		break;
	case BitwiseOperation::BitwiseAnd:
		mag &= b;
		break;
	case BitwiseOperation::BitwiseXor:
		mag ^= b;
		break;
	}
	return fold_unchecked_integer_bits(mag, result_ty);
}

static ConstEvalResult fold_bitwise_and(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_bitwise(result_ty, args, BitwiseOperation::BitwiseAnd);
}

static ConstEvalResult fold_bitwise_or(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_bitwise(result_ty, args, BitwiseOperation::BitwiseOr);
}

static ConstEvalResult fold_bitwise_xor(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_bitwise(result_ty, args, BitwiseOperation::BitwiseXor);
}

static ConstEvalResult fold_logical_not(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0])) {
		return ConstEvalResult::not_constant();
	}
	OrdinalBounds bounds;
	if (!integer_bounds(result_ty, &bounds)) {
		return ConstEvalResult::not_constant();
	}

	// The Delphi operator name is LogicalNot for both Boolean negation and
	// integer complement. For the integer overload, complement exactly the
	// result carrier's width and convert the two's-complement bits back to
	// Integer's magnitude/sign constant representation.
	uint64_t width_value = bounds.signed_type ? bounds.min_magnitude : bounds.max_positive;
	unsigned bits = 0;
	do {
		++bits;
		width_value >>= 1;
	} while (width_value != 0);
	uint64_t mask = bits == 64 ? UINT64_MAX : (uint64_t{1} << bits) - 1;
	const Integer* value = const_integer_arg(args[0]);
	uint64_t raw = value->negative ? (uint64_t{0} - value->value) & mask : value->value & mask;
	uint64_t complemented = (~raw) & mask;
	if (bounds.signed_type) {
		uint64_t sign_bit = uint64_t{1} << (bits - 1);
		if (complemented & sign_bit) {
			uint64_t magnitude = (uint64_t{0} - complemented) & mask;
			return fold_integer_result(magnitude, magnitude != 0, result_ty);
		}
	}
	return fold_integer_result(complemented, false, result_ty);
}

static bool add_u64_checked(uint64_t a, uint64_t b, uint64_t* out) {
	*out = a + b;
	return *out >= a;
}

static ConstEvalResult fold_add_sub(Type* result_ty, const std::vector<Node*>& args, bool subtract) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1])) {
		return ConstEvalResult::not_constant();
	}
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	bool bneg = subtract ? (!b->negative && b->value != 0) : b->negative;
	bool neg = false;
	uint64_t mag = 0;
	if (a->negative == bneg) {
		if (!add_u64_checked(a->value, b->value, &mag)) {
			return ConstEvalResult::error("integer constant overflow");
		}
		neg = a->negative;
	} else if (a->value >= b->value) {
		mag = a->value - b->value;
		neg = a->negative;
	} else {
		mag = b->value - a->value;
		neg = bneg;
	}
	return fold_integer_result(mag, neg && mag != 0, result_ty);
}

static ConstEvalResult fold_add(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_add_sub(result_ty, args, false);
}

static ConstEvalResult fold_subtract(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_add_sub(result_ty, args, true);
}

static ConstEvalResult fold_unchecked_add_sub(Type* result_ty, const std::vector<Node*>& args, bool subtract) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1])) {
		return ConstEvalResult::not_constant();
	}
	uint64_t a = unchecked_integer_bits(const_integer_arg(args[0]));
	uint64_t b = unchecked_integer_bits(const_integer_arg(args[1]));
	return fold_unchecked_integer_bits(subtract ? a - b : a + b, result_ty);
}

static ConstEvalResult fold_unchecked_add(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_unchecked_add_sub(result_ty, args, false);
}

static ConstEvalResult fold_unchecked_subtract(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	return fold_unchecked_add_sub(result_ty, args, true);
}

static ConstEvalResult fold_multiply(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1])) {
		return ConstEvalResult::not_constant();
	}
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	uint64_t mag = 0;
	if (a->value != 0 && b->value > UINT64_MAX / a->value) {
		return ConstEvalResult::error("integer constant overflow");
	}
	mag = a->value * b->value;
	bool neg = (a->negative != b->negative) && mag != 0;
	return fold_integer_result(mag, neg, result_ty);
}

static ConstEvalResult fold_unchecked_multiply(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1])) {
		return ConstEvalResult::not_constant();
	}
	return fold_unchecked_integer_bits(unchecked_integer_bits(const_integer_arg(args[0])) * unchecked_integer_bits(const_integer_arg(args[1])), result_ty);
}

static ConstEvalResult fold_intdivide(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1])) {
		return ConstEvalResult::not_constant();
	}
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	if (b->value == 0) {
		return ConstEvalResult::error("integer constant division by zero");
	}
	uint64_t mag = a->value / b->value;
	bool neg = (a->negative != b->negative) && mag != 0;
	return fold_integer_result(mag, neg, result_ty);
}

static ConstEvalResult fold_unchecked_intdivide(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1])) {
		return ConstEvalResult::not_constant();
	}
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	if (b->value == 0) {
		return ConstEvalResult::error("integer constant division by zero");
	}
	uint64_t magnitude = a->value / b->value;
	bool negative = (a->negative != b->negative) && magnitude != 0;
	return const_explicit_ordinal_cast(magnitude, negative, result_ty);
}

static ConstEvalResult fold_modulus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1])) {
		return ConstEvalResult::not_constant();
	}
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	if (b->value == 0) {
		return ConstEvalResult::error("integer constant modulo by zero");
	}
	uint64_t mag = a->value % b->value;
	// Pascal's integer remainder follows the dividend's sign. For zero, keep the
	// canonical non-negative representation.
	bool neg = a->negative && mag != 0;
	return fold_integer_result(mag, neg, result_ty);
}

static ConstEvalResult fold_length(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !args[0]) {
		return ConstEvalResult::not_constant();
	}
	if (auto string = dynamic_cast<String*>(args[0])) {
		return ConstEvalResult::success(new Integer(string->value.size(), result_ty));
	}
	if (auto array = dynamic_cast<FixedArrayType*>(args[0]->ty)) {
		return ConstEvalResult::success(new Integer(array->range.length, result_ty));
	}
	return ConstEvalResult::not_constant();
}

static ConstEvalResult fold_divide(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2) {
		return ConstEvalResult::not_constant();
	}
	long double a = 0.0, b = 0.0;
	if (!const_numeric_as_long_double(args[0], &a) || !const_numeric_as_long_double(args[1], &b)) {
		return ConstEvalResult::not_constant();
	}
	if (b == 0.0) {
		return ConstEvalResult::error("real constant division by zero");
	}
	return ConstEvalResult::success(new Real(a / b, result_ty));
}

static ConstEvalResult fold_real_to_int64(const std::vector<Node*>& args, bool round) {
	if (args.size() != 1) {
		return ConstEvalResult::not_constant();
	}
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value)) {
		return ConstEvalResult::not_constant();
	}
	long double integral = round ? ::nearbyintl(value) : ::truncl(value);
	constexpr long double limit = 0x1p63L;
	if (!__builtin_isfinite(integral) || integral < -limit || integral >= limit) {
		return ConstEvalResult::error(round ? "Round constant is outside the Int64 range" : "Trunc constant is outside the Int64 range");
	}
	bool negative = integral < 0.0L;
	long double magnitude = negative ? -integral : integral;
	return fold_integer_result(static_cast<uint64_t>(magnitude), negative, int64_type());
}

static ConstEvalResult fold_trunc(ConstEvalContext&, Type*, const std::vector<Node*>& args) {
	return fold_real_to_int64(args, false);
}

static ConstEvalResult fold_round(ConstEvalContext&, Type*, const std::vector<Node*>& args) {
	return fold_real_to_int64(args, true);
}

static ConstEvalResult fold_frac(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1) {
		return ConstEvalResult::not_constant();
	}
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value)) {
		return ConstEvalResult::not_constant();
	}
	long double integral = 0.0L;
	return ConstEvalResult::success(new Real(::modfl(value, &integral), result_ty));
}

static ConstEvalResult fold_sqrt(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1) {
		return ConstEvalResult::not_constant();
	}
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value)) {
		return ConstEvalResult::not_constant();
	}
	return ConstEvalResult::success(new Real(::sqrtl(value), result_ty));
}

static ConstEvalResult fold_sqr(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1) {
		return ConstEvalResult::not_constant();
	}
	if (auto value = const_integer_arg(args[0])) {
		return fold_unchecked_integer_bits(unchecked_integer_bits(value) * unchecked_integer_bits(value), result_ty);
	}
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value)) {
		return ConstEvalResult::not_constant();
	}
	return ConstEvalResult::success(new Real(value * value, result_ty));
}

static ConstEvalResult fold_exp(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1) {
		return ConstEvalResult::not_constant();
	}
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value)) {
		return ConstEvalResult::not_constant();
	}
	return ConstEvalResult::success(new Real(::expl(value), result_ty));
}

static ConstEvalResult fold_ln(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1) {
		return ConstEvalResult::not_constant();
	}
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value)) {
		return ConstEvalResult::not_constant();
	}
	return ConstEvalResult::success(new Real(::logl(value), result_ty));
}

static ConstEvalResult fold_pos(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2) {
		return ConstEvalResult::not_constant();
	}
	auto needle = dynamic_cast<const String*>(args[0]);
	auto haystack = dynamic_cast<const String*>(args[1]);
	if (!needle || !haystack) {
		return ConstEvalResult::not_constant();
	}
	std::size_t found = haystack->value.find(needle->value);
	uint64_t pascal_index = found == std::string::npos ? 0 : static_cast<uint64_t>(found + 1);
	return fold_integer_result(pascal_index, false, result_ty);
}

static ConstEvalResult fold_chr(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1) {
		return ConstEvalResult::not_constant();
	}
	auto value = dynamic_cast<const Integer*>(args[0]);
	if (!value || value->negative || value->value > 255) {
		return ConstEvalResult::not_constant();
	}
	return ConstEvalResult::success(new String(std::string(1, static_cast<char>(static_cast<unsigned char>(value->value))), result_ty));
}

// Pascal-visible builtin procedures/functions. To add one: append a row
// AND implement `::u_system::p_<name>` in rtl.h. Linker enforces the rtl.h side.
static const BuiltinDesc k_builtins[] = {
    {"::u_system::p_ord", fold_ord, {}, BuiltinGenericKind::OrdinalValue},
    // Both operations have the same generic Pascal signature and type
    // relationship; only their ordinary RTL function bodies differ.
    {"::u_system::p_include", nullptr, {}, BuiltinGenericKind::SetMutation},
    {"::u_system::p_exclude", nullptr, {}, BuiltinGenericKind::SetMutation},
    {
        .cxx_name = "::u_system::p_str",
        .const_fold = nullptr,
        .generic_kind = BuiltinGenericKind::StrOutput,
        .syntax_kind = BuiltinSyntaxKind::Str,
    },
    {
        .cxx_name = "::u_system::p_val",
        .const_fold = nullptr,
        .generic_kind = BuiltinGenericKind::ValOutput,
    },
    {"::u_system::p_octstr", nullptr},
    {"::u_system::p_strlen", nullptr},
    {
        .cxx_name = "::u_system::p_new",
        .const_fold = nullptr,
        .syntax_kind = BuiltinSyntaxKind::NewValue,
    },
    {
        .cxx_name = "::u_system::p_dispose",
        .const_fold = nullptr,
        .syntax_kind = BuiltinSyntaxKind::DisposeValue,
    },
    {
        .cxx_name = "::u_system::p_getmem",
        .const_fold = nullptr,
        .generic_kind = BuiltinGenericKind::PointerStorage,
    },
    {"::u_system::p_allocmem", nullptr},
    {
        .cxx_name = "::u_system::p_reallocmem",
        .const_fold = nullptr,
        .generic_kind = BuiltinGenericKind::PointerStorage,
    },
    {"::u_system::p_freemem", nullptr},
    {
        .cxx_name = "::u_system::p_rewrite",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_rewrite",
    },
    {"::u_system::m_unchecked_rewrite", nullptr},
    {
        .cxx_name = "::u_system::p_reset",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_reset",
    },
    {"::u_system::m_unchecked_reset", nullptr},
    {
        .cxx_name = "::u_system::p_close",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_close",
    },
    {"::u_system::m_unchecked_close", nullptr},
    {
        .cxx_name = "::u_system::p_seek",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_seek",
    },
    {"::u_system::m_unchecked_seek", nullptr},
    {
        .cxx_name = "::u_system::p_filepos",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_filepos",
    },
    {"::u_system::m_unchecked_filepos", nullptr},
    {
        .cxx_name = "::u_system::p_filesize",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_filesize",
    },
    {"::u_system::m_unchecked_filesize", nullptr},
    {
        .cxx_name = "::u_system::p_eof",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_eof",
    },
    {"::u_system::m_unchecked_eof", nullptr},
    {
        .cxx_name = "::u_system::p_truncate",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_truncate",
    },
    {"::u_system::m_unchecked_truncate", nullptr},
    {"::u_system::p_ioresult", nullptr},
    {
        .cxx_name = "::u_system::p_blockread",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_blockread",
    },
    {"::u_system::m_unchecked_blockread", nullptr},
    {
        .cxx_name = "::u_system::p_blockwrite",
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_blockwrite",
    },
    {"::u_system::m_unchecked_blockwrite", nullptr},
    {"::u_system::p_halt", nullptr},
    {"::u_system::p_runerror", nullptr},
    // Unqualified because these `m_` names are internal call-site macros, not
    // namespace members or addressable `p_` functions. Their System
    // declarations remain ordinary calls; preprocessing performs the required
    // call-site-sensitive lowering in generated C++.
    {"m_get_frame", nullptr},
    {"m_get_caller_addr", nullptr},
    {"m_get_caller_frame", nullptr},
    {"::u_system::p_low", nullptr, TypeBoundKind::Low},
    {"::u_system::p_high", nullptr, TypeBoundKind::High},
    {
        .cxx_name = "::u_system::p_sizeof",
        .const_fold = nullptr,
        .syntax_kind = BuiltinSyntaxKind::SizeOf,
    },
    {
        .cxx_name = "::u_system::p_write",
        .const_fold = nullptr,
        .syntax_kind = BuiltinSyntaxKind::Write,
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_write",
    },
    {"::u_system::m_unchecked_write", nullptr},
    {
        .cxx_name = "::u_system::p_writeln",
        .const_fold = nullptr,
        .syntax_kind = BuiltinSyntaxKind::WriteLn,
        .call_site_switch = BuiltinCallSiteSwitch::Io,
        .disabled_cxx_name = "::u_system::m_unchecked_writeln",
    },
    {"::u_system::m_unchecked_writeln", nullptr},
    {"::u_system::p_setlength", nullptr, {}, BuiltinGenericKind::SequenceResize},
    {"::u_system::p_uniquestring", nullptr},
    {"::u_system::m_new_instance", nullptr},
    {
        .cxx_name = "::u_system::m_free_object",
        .const_fold = nullptr,
        .call_convention = BuiltinCallConvention::ReceiverFirst,
    },
    {
        .cxx_name = "::u_system::p_length",
        .const_fold = fold_length,
        .generic_kind = BuiltinGenericKind::SequenceLength,
    },
    {"::u_system::p_index", nullptr},
    {"::u_system::m_unchecked_index", nullptr},
    {"::u_system::tpcc_index_write", nullptr},
    {"::u_system::p_chr", fold_chr},
    {"::u_system::p_fillchar", nullptr},
    {"::u_system::p_fillbyte", nullptr},
    {"::u_system::p_filldword", nullptr},
    {"::u_system::p_prefetch", nullptr},
    {"::u_system::p_move", nullptr},
    {"::u_system::p_initialize", nullptr},
    {"::u_system::p_finalize", nullptr},
    {"::u_system::p_comparebyte", nullptr},
    {"::u_system::p_comparechar", nullptr},
    {"::u_system::p_assigned", fold_assigned, {}, BuiltinGenericKind::Assigned},
    {
        .cxx_name = "::u_system::p_abs",
        .const_fold = fold_abs,
        .generic_kind = BuiltinGenericKind::AbsoluteValue,
        .call_site_switch = BuiltinCallSiteSwitch::Overflow,
        .disabled_cxx_name = "::u_system::m_unchecked_abs",
    },
    {
        .cxx_name = "::u_system::m_unchecked_abs",
        .const_fold = fold_unchecked_abs,
        .generic_kind = BuiltinGenericKind::AbsoluteValue,
    },
    {
        .cxx_name = "::u_system::p_succ",
        .const_fold = fold_succ,
        .generic_kind = BuiltinGenericKind::OrdinalSuccessorOrPredecessor,
        .call_site_switch = BuiltinCallSiteSwitch::Overflow,
        .disabled_cxx_name = "::u_system::m_unchecked_succ",
    },
    {
        .cxx_name = "::u_system::m_unchecked_succ",
        .const_fold = fold_unchecked_succ,
        .generic_kind = BuiltinGenericKind::OrdinalSuccessorOrPredecessor,
    },
    {
        .cxx_name = "::u_system::p_pred",
        .const_fold = fold_pred,
        .generic_kind = BuiltinGenericKind::OrdinalSuccessorOrPredecessor,
        .call_site_switch = BuiltinCallSiteSwitch::Overflow,
        .disabled_cxx_name = "::u_system::m_unchecked_pred",
    },
    {
        .cxx_name = "::u_system::m_unchecked_pred",
        .const_fold = fold_unchecked_pred,
        .generic_kind = BuiltinGenericKind::OrdinalSuccessorOrPredecessor,
    },
    {"::u_system::p_trunc", fold_trunc},
    {"::u_system::p_round", fold_round},
    {"::u_system::p_frac", fold_frac},
    {"::u_system::p_sqr", fold_sqr},
    {"::u_system::p_sqrt", fold_sqrt},
    {"::u_system::p_exp", fold_exp},
    {"::u_system::p_ln", fold_ln},
    {"::u_system::p_pos", fold_pos},
    {"::u_system::p_copy", nullptr},
    {
        .cxx_name = "::u_system::p_delete",
        .const_fold = nullptr,
        .generic_kind = BuiltinGenericKind::ShortStringMutation,
    },
    {
        .cxx_name = "::u_system::p_insert",
        .const_fold = nullptr,
        .generic_kind = BuiltinGenericKind::ShortStringMutation,
    },
    // TODO: Delphi has operators "explicit", "implicit".

    {"::u_system::o_bitwiseand", fold_bitwise_and},
    {"::u_system::o_bitwiseor", fold_bitwise_or},
    {"::u_system::o_bitwisexor", fold_bitwise_xor},

    // Delphi {"::u_system::p_logicalor", nullptr},
    // Delphi {"::u_system::p_logicaland", nullptr},
    {"::u_system::o_logicalnot", fold_logical_not},
    {"::u_system::o_logicalxor", nullptr},

    {"::u_system::o_unchecked_add", fold_unchecked_add},
    {"::u_system::o_add", fold_add},
    {"::u_system::o_unchecked_subtract", fold_unchecked_subtract},
    {"::u_system::o_subtract", fold_subtract},
    {"::u_system::o_positive", fold_unary_plus},
    {"::u_system::o_unchecked_negative", fold_unchecked_unary_minus},
    {"::u_system::o_negative", fold_unary_minus},
    {"::u_system::o_unchecked_multiply", fold_unchecked_multiply},
    {"::u_system::o_multiply", fold_multiply},
    {"::u_system::o_divide", fold_divide},
    {"::u_system::o_unchecked_intdivide", fold_unchecked_intdivide},
    {"::u_system::o_intdivide", fold_intdivide},
    {"::u_system::o_implicit", fold_implicit},
    // Old-style file Assign is an ordinary procedure, not an implicit
    // conversion despite sharing the Pascal spelling "assign".
    {"::u_system::p_assign", nullptr},
    {"::u_system::o_modulus", fold_modulus},
    {"::u_system::o_leftshift", fold_leftshift},
    {"::u_system::o_rightshift", fold_rightshift},

    {"::u_system::o_lessthan", fold_lessthan},
    {"::u_system::o_lessthanorequal", fold_lessthanorequal},
    {"::u_system::o_equal", fold_equal},
    {"::u_system::o_greaterthan", fold_greaterthan},
    {"::u_system::o_greaterthanorequal", fold_greaterthanorequal},
    {"::u_system::o_in", nullptr, {}, BuiltinGenericKind::SetMembership},
    {"::u_system::p_supports", nullptr},

    {"::u_system::t_boolean::p_true", nullptr},
    {"::u_system::t_boolean::p_false", nullptr},
};

// These descriptors belong only to the generic root-frame fallbacks below.
// They are deliberately not in k_builtins: concrete System arithmetic
// declarations use the same C++ operation names but have complete Pascal
// signatures and must not be mistaken for omitted-type generic declarations.
static const BuiltinDesc k_checked_inc_fallback{"::u_system::o_inc", nullptr, {}, BuiltinGenericKind::UnaryOrdinalOrPointerStep};
static const BuiltinDesc k_unchecked_inc_fallback{"::u_system::o_unchecked_inc", nullptr, {}, BuiltinGenericKind::UnaryOrdinalOrPointerStep};
static const BuiltinDesc k_checked_dec_fallback{"::u_system::o_dec", nullptr, {}, BuiltinGenericKind::UnaryOrdinalOrPointerStep};
static const BuiltinDesc k_unchecked_dec_fallback{"::u_system::o_unchecked_dec", nullptr, {}, BuiltinGenericKind::UnaryOrdinalOrPointerStep};
static const BuiltinDesc k_checked_add_fallback{"::u_system::o_add", nullptr, {}, BuiltinGenericKind::EnumOrPointerStep};
static const BuiltinDesc k_unchecked_add_fallback{"::u_system::o_unchecked_add", nullptr, {}, BuiltinGenericKind::EnumOrPointerStep};
static const BuiltinDesc k_checked_subtract_fallback{"::u_system::o_subtract", nullptr, {}, BuiltinGenericKind::EnumOrPointerStep};
static const BuiltinDesc k_unchecked_subtract_fallback{"::u_system::o_unchecked_subtract", nullptr, {}, BuiltinGenericKind::EnumOrPointerStep};
static const BuiltinDesc k_checked_pointer_difference_fallback{"::u_system::o_subtract", nullptr, {}, BuiltinGenericKind::PointerDifference};
static const BuiltinDesc k_unchecked_pointer_difference_fallback{"::u_system::o_unchecked_subtract", nullptr, {}, BuiltinGenericKind::PointerDifference};
static const BuiltinDesc k_checked_set_union_fallback{"::u_system::o_add", fold_set_union, {}, BuiltinGenericKind::SetUnionOrDifference};
static const BuiltinDesc k_unchecked_set_union_fallback{"::u_system::o_unchecked_add", fold_set_union, {}, BuiltinGenericKind::SetUnionOrDifference};
static const BuiltinDesc k_checked_set_difference_fallback{"::u_system::o_subtract", fold_set_difference, {}, BuiltinGenericKind::SetUnionOrDifference};
static const BuiltinDesc k_unchecked_set_difference_fallback{"::u_system::o_unchecked_subtract", fold_set_difference, {}, BuiltinGenericKind::SetUnionOrDifference};
static const BuiltinDesc k_enum_equal_fallback{"::u_system::o_equal", fold_equal, {}, BuiltinGenericKind::EnumEquality};

Type* lookup_builtin_type(std::string cxx_name) {
	if (cxx_name == "::u_system::t_tmethod") {
		return tmethod_type();
	}
	// FIXME: A 32-bit -P target must map the signed names to LongInt and the
	// unsigned names to LongWord. They are aliases, so lookup returns the
	// canonical Type* rather than manufacturing four nominal intrinsics.
	if (cxx_name == "::u_system::t_ptrint" || cxx_name == "::u_system::t_sizeint") {
		return int64_type();
	}
	if (cxx_name == "::u_system::t_ptruint" || cxx_name == "::u_system::t_sizeuint") {
		return qword_type();
	}
	for (auto t : k_all_intrinsics) {
		if (auto q = dynamic_cast<IntrinsicType*>(t)) {
			if (q->cxx_name == cxx_name) {
				return t;
			}
		} else if (auto q = dynamic_cast<ShortStringType*>(t)) {
			if (q->capacity == 255 && cxx_name == "::u_system::t_shortstring<255>") {
				return q;
			}
		} else if (auto q = dynamic_cast<InterfaceType*>(t)) {
			if (q->cxx_name == cxx_name) {
				return q;
			}
		} else if (auto q = dynamic_cast<EnumType*>(t)) {
			if (q->cxx_name == cxx_name) {
				return q;
			}
		} else if (auto q = dynamic_cast<RecordType*>(t)) {
			if (q->cxx_name == cxx_name) {
				return q;
			}
		} else if (auto q = dynamic_cast<PointerType*>(t)) {
			if (q->is_untyped() && q->cxx_name == cxx_name) {
				return q;
			}
		}
	}
	return nullptr;
}

const BuiltinDesc* lookup_builtin_desc(std::string_view cxx_name) {
	for (auto& b : k_builtins) {
		if (b.cxx_name == cxx_name) {
			return &b;
		}
	}
	return nullptr;
}

Builtin* create_builtin_value(std::string cxx_name) {
	for (auto& b : k_builtins) {
		if (b.cxx_name == cxx_name) {
			auto bi = new Builtin(&b);
			return bi;
		}
	}
	// Fallback to anything, we will get a linker error anyway.
	fprintf(stderr, "warning: builtin '%s' doesn't have a registration.  Allowing it--but it won't constant-fold.\n", cxx_name.c_str());
	auto bs = BuiltinDesc{cxx_name, nullptr};
	auto bi = new Builtin(&bs);
	return bi;
}

const Frame& root_frame() {
	static const Frame f = []() {
		Frame ff(nullptr);
		auto p_false = new EnumMemberRef("::u_system::t_boolean::p_false", 0, &k_boolean);
		ff.register_variable("false", p_false, &k_boolean);
		auto p_true = new EnumMemberRef("::u_system::t_boolean::p_true", 1, &k_boolean);
		ff.register_variable("true", p_true, &k_boolean);
		// File is a reserved type-forming keyword, not a declaration in
		// System. Registering its singleton here also lets type-or-expression
		// syntax such as SizeOf(File) recognize it as a type.
		ff.register_type("file", &k_file);

		// Abs, Succ, and Pred are ordinary Pascal function calls, so their
		// declarations live under their source names and participate in the
		// existing frame lookup and overload selection. Their exact result
		// relationships cannot currently be written as Pascal declarations:
		// Abs is T -> T over predefined numeric types, while Succ/Pred are
		// T -> T for every intrinsic ordinal, enum, and subrange definition.
		// An unknown formal/result expresses only that missing quantification.
		// Generic ranking makes these compiler declarations lose to every
		// complete user or System overload; no separate intrinsic lookup is
		// involved.
		auto register_generic_unary = [&ff](std::string_view pas_name, const BuiltinDesc* descriptor) {
			std::vector<Parameter> formals;
			formals.emplace_back("value", "p_value", unknown_type(), ParamMode::Value, nullptr);
			auto routine_type = new RoutineType(SourceLocation::builtin(), std::move(formals), unknown_type(), ROUTINE);
			auto procedure = new Procedure(std::string(descriptor->cxx_name), std::string(pas_name), routine_type, true);
			procedure->builtin_desc = descriptor;
			procedure->is_external = true;
			procedure->has_body = true;
			auto registration = ff.register_callable(std::string(pas_name), procedure);
			assert(registration.kind == CallableRegistration::Kind::Added);
		};
		register_generic_unary("abs", lookup_builtin_desc("::u_system::p_abs"));
		register_generic_unary("succ", lookup_builtin_desc("::u_system::p_succ"));
		register_generic_unary("pred", lookup_builtin_desc("::u_system::p_pred"));

		// Inc/Dec have one exact operation for every ordinal and pointer type,
		// and their distance forms need the corresponding otherwise-infinite
		// Add/Subtract family. Pascal currently has no generic declaration
		// syntax capable of spelling those T -> T and (T, Integer) -> T
		// contracts. Represent only that missing declaration relation here:
		// these are ordinary Procedure values in the ordinary root Frame, and
		// their omitted first formals use the existing Generic match rank so a
		// typed System or user declaration always wins without a secondary
		// resolver.
		auto register_step = [&ff](OperatorInvocation invocation, std::string_view spelling, std::size_t arity, bool checked, const BuiltinDesc* descriptor) {
			auto identifier = operator_invocation_identifier(invocation, spelling, arity, checked, false);
			assert(identifier);
			std::vector<Parameter> formals;
			formals.emplace_back("value", "p_value", unknown_type(), ParamMode::Value, nullptr);
			if (arity == 2) {
				formals.emplace_back("amount", "p_amount", unknown_type(), ParamMode::Value, nullptr);
			}
			auto routine_type = new RoutineType(SourceLocation::builtin(), std::move(formals), unknown_type(), ROUTINE);
			auto procedure = new Procedure(std::string(descriptor->cxx_name), std::string(*identifier), routine_type, true);
			procedure->builtin_desc = descriptor;
			procedure->is_external = true;
			procedure->has_body = true;
			auto registration = ff.register_callable(std::string(*identifier), procedure);
			assert(registration.kind == CallableRegistration::Kind::Added);
		};
		register_step(OperatorInvocation::MutatingUnary, "inc", 1, true, &k_checked_inc_fallback);
		register_step(OperatorInvocation::MutatingUnary, "inc", 1, false, &k_unchecked_inc_fallback);
		register_step(OperatorInvocation::MutatingUnary, "dec", 1, true, &k_checked_dec_fallback);
		register_step(OperatorInvocation::MutatingUnary, "dec", 1, false, &k_unchecked_dec_fallback);
		register_step(OperatorInvocation::BinaryToken, "+", 2, true, &k_checked_add_fallback);
		register_step(OperatorInvocation::BinaryToken, "+", 2, false, &k_unchecked_add_fallback);
		register_step(OperatorInvocation::BinaryToken, "-", 2, true, &k_checked_subtract_fallback);
		register_step(OperatorInvocation::BinaryToken, "-", 2, false, &k_unchecked_subtract_fallback);

		// Enum equality is otherwise unspellable in system.pp. Register the
		// omitted-formal = candidate in the root frame; the EnumEquality
		// descriptor validates the relation after selection, so concrete
		// System/user overloads still win normally. Records are excluded:
		// FPC has no built-in record equality (a user `operator =` is
		// required), so this fallback must not invent one.
		register_step(OperatorInvocation::BinaryToken, "=", 2, true, &k_enum_equal_fallback);

		// Pascal cannot declare `(set of T, set of T) -> set of T` without
		// generic routine syntax. A set-of-unknown placeholder gives these
		// candidates distinct ordinary overload signatures; their descriptor
		// later contextualizes bracket literals and restores one concrete set
		// type without adding another lookup path.
		auto generic_set = new FixedSetType(SourceLocation::builtin(), unknown_type());
		struct SetOperation {
			std::string_view spelling;
			bool checked;
			const BuiltinDesc* descriptor;
		};
		for (const SetOperation& operation : std::array{
		         SetOperation{"+", true, &k_checked_set_union_fallback},
		         SetOperation{"+", false, &k_unchecked_set_union_fallback},
		         SetOperation{"-", true, &k_checked_set_difference_fallback},
		         SetOperation{"-", false, &k_unchecked_set_difference_fallback},
		     }) {
			auto identifier = operator_invocation_identifier(OperatorInvocation::BinaryToken, operation.spelling, 2, operation.checked, false);
			assert(identifier);
			std::vector<Parameter> formals;
			formals.emplace_back("first", "p_first", generic_set, ParamMode::Const, nullptr);
			formals.emplace_back("second", "p_second", generic_set, ParamMode::Const, nullptr);
			auto routine_type = new RoutineType(SourceLocation::builtin(), std::move(formals), unknown_type(), ROUTINE);
			auto procedure = new Procedure(std::string(operation.descriptor->cxx_name), std::string(*identifier), routine_type, true);
			procedure->builtin_desc = operation.descriptor;
			procedure->is_external = true;
			procedure->has_body = true;
			auto registration = ff.register_callable(std::string(*identifier), procedure);
			assert(registration.kind == CallableRegistration::Kind::Added);
		}

		// Pointer subtraction is a second ordinary overload, not the
		// pointer-minus-integer step above. Pascal cannot quantify one pointee
		// type across both operands, while spelling `(Pointer, Pointer)` in
		// System would erase the element size before the RTL call. Give the
		// root declaration a distinct ordinary signature and let its
		// PointerDifference descriptor validate/preserve the actual ^T pair.
		for (auto [checked, descriptor] : std::array{
		         std::pair{true, &k_checked_pointer_difference_fallback},
		         std::pair{false, &k_unchecked_pointer_difference_fallback},
		     }) {
			auto identifier = operator_invocation_identifier(OperatorInvocation::BinaryToken, "-", 2, checked, false);
			assert(identifier);
			std::vector<Parameter> formals;
			formals.emplace_back("first", "p_first", pointer_type(), ParamMode::Value, nullptr);
			formals.emplace_back("second", "p_second", pointer_type(), ParamMode::Value, nullptr);
			auto routine_type = new RoutineType(SourceLocation::builtin(), std::move(formals), ptrint_type(), ROUTINE);
			auto procedure = new Procedure(std::string(descriptor->cxx_name), std::string(*identifier), routine_type, true);
			procedure->builtin_desc = descriptor;
			procedure->is_external = true;
			procedure->has_body = true;
			auto registration = ff.register_callable(std::string(*identifier), procedure);
			assert(registration.kind == CallableRegistration::Kind::Added);
		}
		return ff;
	}();
	return f;
}

#include "diagnostic.h"

const char* IntrinsicType::diagnostic_kind() const {
	return "intrinsic";
}

void IntrinsicType::collect_diagnostic_edges(ErrorLetContext*) const {
}

void IntrinsicType::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	// IntrinsicType is a compiler-provided Pascal-visible type. Do not print the
	// C++ carrier name here, and do not infer a semantic family from the widening
	// rank: the rank is only overload/conversion ordering metadata.
	if (rank) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "widening_rank: " << *rank;
	}
}

const char* Builtin::diagnostic_kind() const {
	return "builtin";
}

void Builtin::collect_diagnostic_edges(ErrorLetContext*) const {
	// A Builtin denotes an opaque C++ overload set such as
	// ::u_system::p_include, not one Pascal RoutineType. Do not add Node::ty
	// here; it is intentionally null.
}

void Builtin::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	// Builtin::desc names the C++ implementation hook. Diagnostics describe the
	// source-visible value instead; the let binding carries the Pascal name when
	// the builtin is referenced from a scope.
	out << "builtin\n";
	ctx->indent(out, indent + 1);
	out << "signature: '<builtin overload set>'";
}
