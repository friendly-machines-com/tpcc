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

IntrinsicType::IntrinsicType(SourceLocation source_location,
			     std::string cxx_name,
			     std::optional<int> rank,
			     std::optional<OrdinalBounds> ordinal_bounds,
			     std::optional<TypeLayout> layout,
			     std::optional<IntrinsicCarrier> carrier)
    : Type(std::move(source_location)),
      cxx_name(std::move(cxx_name)),
      rank(std::move(rank)),
      ordinal_bounds(std::move(ordinal_bounds)),
      layout(std::move(layout)),
      carrier(std::move(carrier)) {}

Builtin::Builtin(const BuiltinDesc* desc) : desc(desc) {}

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
EnumType k_boolean(
    SourceLocation::builtin(),
    "::u_system::t_boolean", "false", "true",
    8, false);
IntrinsicType k_char(SourceLocation::builtin(), "::u_system::t_char", {}, unsigned_bounds(8), TypeLayout{1, 1}, IntrinsicCarrier::Character);
ShortStringType k_shortstring(SourceLocation::builtin(), 255);
IntrinsicType k_ansistring(SourceLocation::builtin(), "::u_system::t_ansistring", {}, {}, TypeLayout{8, 8}, IntrinsicCarrier::AnsiString);
IntrinsicType k_text(SourceLocation::builtin(), "::u_system::t_text", {}, {}, TypeLayout{8, 8}, IntrinsicCarrier::Text);
IntrinsicType k_file(SourceLocation::builtin(), "::u_system::t_file", {}, {}, TypeLayout{8, 8}, IntrinsicCarrier::File);
PointerType k_pointer(
    SourceLocation::builtin(), nullptr,
    "::u_system::t_pointer");
IntrinsicType k_ptrint(SourceLocation::builtin(), "::u_system::t_ptrint", 8, signed_bounds(64), TypeLayout{8, 8}, IntrinsicCarrier::Int64);
IntrinsicType k_ptruint(SourceLocation::builtin(), "::u_system::t_ptruint", 7, unsigned_bounds(64), TypeLayout{8, 8}, IntrinsicCarrier::UInt64);
IntrinsicType k_sizeint(SourceLocation::builtin(), "::u_system::t_sizeint", 8, signed_bounds(64), TypeLayout{8, 8}, IntrinsicCarrier::Int64);
IntrinsicType k_sizeuint(SourceLocation::builtin(), "::u_system::t_sizeuint", 7, unsigned_bounds(64), TypeLayout{8, 8}, IntrinsicCarrier::UInt64);
IntrinsicType k_fixedarray(SourceLocation::builtin(), "::u_system::t_fixedarray", {});
IntrinsicType k_unknown(SourceLocation::builtin(), "::u_system::tpcc_unknown_type", {});

struct TMethodDefinition {
	Frame children;
	RecordType type;
	StorageSlot code;
	StorageSlot data;

	TMethodDefinition()
	    : children(nullptr),
	      type(SourceLocation::builtin(),
		   &children),
	      code("p_code", &k_pointer),
	      data("p_data", &k_pointer) {
		type.cxx_name =
		    "::u_system::t_tmethod";
		children.register_variable(
		    "code", &code, &k_pointer);
		children.register_variable(
		    "data", &data, &k_pointer);
		type.fields.push_back(
		    AggregateField{
			"code", &code, &k_pointer});
		type.fields.push_back(
		    AggregateField{
			"data", &data, &k_pointer});
	}
};

TMethodDefinition& tmethod_definition() {
	static TMethodDefinition definition;
	return definition;
}

Type* const k_all_intrinsics[] = {
    &k_byte,
    &k_shortint,
    &k_word,
    &k_smallint,
    &k_longword,
    &k_integer,
    &k_longint,
    &k_qword,
    &k_int64,
    &k_set,
    &k_single,
    &k_double,
    &k_extended,
    &k_boolean,
    &k_char,
    &k_shortstring,
    &k_ansistring,
    &k_text,
    &k_file,
    &k_pointer,
    &k_ptrint,
    &k_ptruint,
    &k_sizeint,
    &k_sizeuint,
    &k_fixedarray,
    &k_unknown,
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

Type* byte_type() { return &k_byte; }
Type* shortint_type() { return &k_shortint; }
Type* word_type() { return &k_word; }
Type* smallint_type() { return &k_smallint; }
Type* cardinal_type() { return &k_longword; }
Type* integer_type() { return &k_integer; }
Type* longint_type() { return &k_longint; }
Type* sizeint_type() { return &k_sizeint; }
Type* qword_type() { return &k_qword; }
Type* int64_type() { return &k_int64; }
Type* pointer_type() { return &k_pointer; }
Type* ptrint_type() { return &k_ptrint; }
Type* ptruint_type() { return &k_ptruint; }
Type* boolean_type() { return &k_boolean; }
Type* char_type() { return &k_char; }
ShortStringType* shortstring_type(uint8_t capacity) {
	if (capacity == 255)
		return &k_shortstring;
	static std::array<ShortStringType*, 256> types{};
	ShortStringType*& result = types[capacity];
	if (!result)
		result = new ShortStringType(SourceLocation::builtin(), capacity);
	return result;
}
Type* ansistring_type() { return &k_ansistring; }
Type* text_type() { return &k_text; }
Type* file_type() { return &k_file; }
Type* single_type() { return &k_single; }
Type* double_type() { return &k_double; }
Type* extended_type() { return &k_extended; }
Type* set_type() { return &k_set; }
Type* fixedarray_type() { return &k_fixedarray; }
Type* unknown_type() { return &k_unknown; }
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
	auto intrinsic =
	    dynamic_cast<const IntrinsicType*>(ty);
	if (!intrinsic || !intrinsic->ordinal_bounds)
		return false;
	*out = *intrinsic->ordinal_bounds;
	return true;
}

Type* IntrinsicType::sequence_element_type() const {
	return carrier == IntrinsicCarrier::AnsiString
		   ? char_type()
		   : nullptr;
}

Type* IntrinsicType::sequence_index_type() const {
	return carrier == IntrinsicCarrier::AnsiString
		   ? integer_type()
		   : nullptr;
}

Type* IntrinsicType::sequence_length_type() const {
	return carrier == IntrinsicCarrier::AnsiString
		   ? sizeint_type()
		   : nullptr;
}

bool IntrinsicType::sequence_is_resizable() const {
	return carrier == IntrinsicCarrier::AnsiString;
}

bool IntrinsicType::has_managed_lifetime() const {
	return carrier == IntrinsicCarrier::AnsiString;
}

bool integer_bounds(
    const Type* ty, OrdinalBounds* out) {
	auto intrinsic =
	    dynamic_cast<const IntrinsicType*>(ty);
	if (!intrinsic || !intrinsic->rank || !intrinsic->ordinal_bounds)
		return false;
	*out = *intrinsic->ordinal_bounds;
	return true;
}

static const Integer* const_integer_arg(Node* n) { return dynamic_cast<const Integer*>(n); }

static bool const_numeric_as_long_double(Node* n, long double* out) {
	if (auto i = dynamic_cast<const Integer*>(n)) {
		*out = static_cast<long double>(i->value);
		if (i->negative)
			*out = -*out;
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

static uint64_t unchecked_integer_bits(
    const Integer* value) {
	return value->negative
		   ? uint64_t{0} - value->value
		   : value->value;
}

static ConstEvalResult fold_unchecked_integer_bits(
    uint64_t bits, Type* result_ty) {
	// The explicit ordinal cast is TPCC's existing representation conversion:
	// it truncates to the Pascal carrier width and then interprets that bit
	// pattern with the carrier's signedness.
	return const_explicit_ordinal_cast(
	    bits, false, result_ty);
}

static ConstEvalResult fold_unary_minus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0]))
		return ConstEvalResult::not_constant();
	auto i = const_integer_arg(args[0]);
	return fold_integer_result(i->value, !i->negative && i->value != 0, result_ty);
}

static ConstEvalResult fold_unchecked_unary_minus(
    ConstEvalContext&, Type* result_ty,
    const std::vector<Node*>& args) {
	if (args.size() != 1 ||
	    !const_integer_arg(args[0]))
		return ConstEvalResult::not_constant();
	return fold_unchecked_integer_bits(
	    uint64_t{0} -
		unchecked_integer_bits(
		    const_integer_arg(args[0])),
	    result_ty);
}

static ConstEvalResult fold_unary_plus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0]))
		return ConstEvalResult::not_constant();
	auto i = const_integer_arg(args[0]);
	return fold_integer_result(i->value, i->negative, result_ty);
}

static ConstEvalResult fold_logical_not(
    ConstEvalContext&, Type* result_ty,
    const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0]))
		return ConstEvalResult::not_constant();
	OrdinalBounds bounds;
	if (!integer_bounds(result_ty, &bounds))
		return ConstEvalResult::not_constant();

	// The Delphi operator name is LogicalNot for both Boolean negation and
	// integer complement. For the integer overload, complement exactly the
	// result carrier's width and convert the two's-complement bits back to
	// Integer's magnitude/sign constant representation.
	uint64_t width_value = bounds.signed_type
				   ? bounds.min_magnitude
				   : bounds.max_positive;
	unsigned bits = 0;
	do {
		++bits;
		width_value >>= 1;
	} while (width_value != 0);
	uint64_t mask = bits == 64
			    ? UINT64_MAX
			    : (uint64_t{1} << bits) - 1;
	const Integer* value = const_integer_arg(args[0]);
	uint64_t raw = value->negative
			   ? (uint64_t{0} - value->value) & mask
			   : value->value & mask;
	uint64_t complemented = (~raw) & mask;
	if (bounds.signed_type) {
		uint64_t sign_bit =
		    uint64_t{1} << (bits - 1);
		if (complemented & sign_bit) {
			uint64_t magnitude =
			    (uint64_t{0} - complemented) & mask;
			return fold_integer_result(
			    magnitude, magnitude != 0, result_ty);
		}
	}
	return fold_integer_result(
	    complemented, false, result_ty);
}

static bool add_u64_checked(uint64_t a, uint64_t b, uint64_t* out) {
	*out = a + b;
	return *out >= a;
}

static ConstEvalResult fold_add_sub(Type* result_ty, const std::vector<Node*>& args, bool subtract) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1]))
		return ConstEvalResult::not_constant();
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	bool bneg = subtract ? (!b->negative && b->value != 0) : b->negative;
	bool neg = false;
	uint64_t mag = 0;
	if (a->negative == bneg) {
		if (!add_u64_checked(a->value, b->value, &mag))
			return ConstEvalResult::error("integer constant overflow");
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

static ConstEvalResult fold_add(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) { return fold_add_sub(result_ty, args, false); }
static ConstEvalResult fold_subtract(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) { return fold_add_sub(result_ty, args, true); }

static ConstEvalResult fold_unchecked_add_sub(
    Type* result_ty,
    const std::vector<Node*>& args,
    bool subtract) {
	if (args.size() != 2 ||
	    !const_integer_arg(args[0]) ||
	    !const_integer_arg(args[1]))
		return ConstEvalResult::not_constant();
	uint64_t a =
	    unchecked_integer_bits(
		const_integer_arg(args[0]));
	uint64_t b =
	    unchecked_integer_bits(
		const_integer_arg(args[1]));
	return fold_unchecked_integer_bits(
	    subtract ? a - b : a + b,
	    result_ty);
}

static ConstEvalResult fold_unchecked_add(
    ConstEvalContext&, Type* result_ty,
    const std::vector<Node*>& args) {
	return fold_unchecked_add_sub(
	    result_ty, args, false);
}

static ConstEvalResult fold_unchecked_subtract(
    ConstEvalContext&, Type* result_ty,
    const std::vector<Node*>& args) {
	return fold_unchecked_add_sub(
	    result_ty, args, true);
}

static ConstEvalResult fold_multiply(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1]))
		return ConstEvalResult::not_constant();
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	uint64_t mag = 0;
	if (a->value != 0 && b->value > UINT64_MAX / a->value)
		return ConstEvalResult::error("integer constant overflow");
	mag = a->value * b->value;
	bool neg = (a->negative != b->negative) && mag != 0;
	return fold_integer_result(mag, neg, result_ty);
}

static ConstEvalResult fold_unchecked_multiply(
    ConstEvalContext&, Type* result_ty,
    const std::vector<Node*>& args) {
	if (args.size() != 2 ||
	    !const_integer_arg(args[0]) ||
	    !const_integer_arg(args[1]))
		return ConstEvalResult::not_constant();
	return fold_unchecked_integer_bits(
	    unchecked_integer_bits(
		const_integer_arg(args[0])) *
		unchecked_integer_bits(
		    const_integer_arg(args[1])),
	    result_ty);
}

static ConstEvalResult fold_intdivide(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1]))
		return ConstEvalResult::not_constant();
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	if (b->value == 0)
		return ConstEvalResult::error("integer constant division by zero");
	uint64_t mag = a->value / b->value;
	bool neg = (a->negative != b->negative) && mag != 0;
	return fold_integer_result(mag, neg, result_ty);
}

static ConstEvalResult fold_unchecked_intdivide(
    ConstEvalContext&, Type* result_ty,
    const std::vector<Node*>& args) {
	if (args.size() != 2 ||
	    !const_integer_arg(args[0]) ||
	    !const_integer_arg(args[1]))
		return ConstEvalResult::not_constant();
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	if (b->value == 0)
		return ConstEvalResult::error(
		    "integer constant division by zero");
	uint64_t magnitude = a->value / b->value;
	bool negative =
	    (a->negative != b->negative) &&
	    magnitude != 0;
	return const_explicit_ordinal_cast(
	    magnitude, negative, result_ty);
}

static ConstEvalResult fold_modulus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2 || !const_integer_arg(args[0]) || !const_integer_arg(args[1]))
		return ConstEvalResult::not_constant();
	auto a = const_integer_arg(args[0]);
	auto b = const_integer_arg(args[1]);
	if (b->value == 0)
		return ConstEvalResult::error("integer constant modulo by zero");
	uint64_t mag = a->value % b->value;
	// Pascal's integer remainder follows the dividend's sign. For zero, keep the
	// canonical non-negative representation.
	bool neg = a->negative && mag != 0;
	return fold_integer_result(mag, neg, result_ty);
}

static ConstEvalResult fold_length(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !args[0])
		return ConstEvalResult::not_constant();
	if (auto string = dynamic_cast<String*>(args[0]))
		return ConstEvalResult::success(new Integer(string->value.size(), result_ty));
	if (auto array = dynamic_cast<FixedArrayType*>(args[0]->ty))
		return ConstEvalResult::success(new Integer(array->range.length, result_ty));
	return ConstEvalResult::not_constant();
}

static ConstEvalResult fold_divide(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2)
		return ConstEvalResult::not_constant();
	long double a = 0.0, b = 0.0;
	if (!const_numeric_as_long_double(args[0], &a) || !const_numeric_as_long_double(args[1], &b))
		return ConstEvalResult::not_constant();
	if (b == 0.0)
		return ConstEvalResult::error("real constant division by zero");
	return ConstEvalResult::success(new Real(a / b, result_ty));
}

static ConstEvalResult fold_real_to_int64(const std::vector<Node*>& args, bool round) {
	if (args.size() != 1)
		return ConstEvalResult::not_constant();
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value))
		return ConstEvalResult::not_constant();
	long double integral = round ? ::nearbyintl(value) : ::truncl(value);
	constexpr long double limit = 0x1p63L;
	if (!__builtin_isfinite(integral) || integral < -limit || integral >= limit)
		return ConstEvalResult::error(round
						  ? "Round constant is outside the Int64 range"
						  : "Trunc constant is outside the Int64 range");
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
	if (args.size() != 1)
		return ConstEvalResult::not_constant();
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value))
		return ConstEvalResult::not_constant();
	long double integral = 0.0L;
	return ConstEvalResult::success(new Real(::modfl(value, &integral), result_ty));
}

static ConstEvalResult fold_sqrt(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1)
		return ConstEvalResult::not_constant();
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value))
		return ConstEvalResult::not_constant();
	return ConstEvalResult::success(new Real(::sqrtl(value), result_ty));
}

static ConstEvalResult fold_exp(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1)
		return ConstEvalResult::not_constant();
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value))
		return ConstEvalResult::not_constant();
	return ConstEvalResult::success(new Real(::expl(value), result_ty));
}

static ConstEvalResult fold_ln(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1)
		return ConstEvalResult::not_constant();
	long double value = 0.0L;
	if (!const_numeric_as_long_double(args[0], &value))
		return ConstEvalResult::not_constant();
	return ConstEvalResult::success(new Real(::logl(value), result_ty));
}

static ConstEvalResult fold_pos(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 2)
		return ConstEvalResult::not_constant();
	auto needle = dynamic_cast<const String*>(args[0]);
	auto haystack = dynamic_cast<const String*>(args[1]);
	if (!needle || !haystack)
		return ConstEvalResult::not_constant();
	std::size_t found = haystack->value.find(needle->value);
	uint64_t pascal_index = found == std::string::npos ? 0 : static_cast<uint64_t>(found + 1);
	return fold_integer_result(pascal_index, false, result_ty);
}

static ConstEvalResult fold_chr(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1)
		return ConstEvalResult::not_constant();
	auto value = dynamic_cast<const Integer*>(args[0]);
	if (!value || value->negative || value->value > 255)
		return ConstEvalResult::not_constant();
	return ConstEvalResult::success(new String(
	    std::string(1, static_cast<char>(static_cast<unsigned char>(value->value))),
	    result_ty));
}

// Pascal-visible builtin procedures/functions. To add one: append a row
// AND implement `::u_system::p_<name>` in rtl.h. Linker enforces the rtl.h side.
static const BuiltinDesc k_builtins[] = {
    {"::u_system::p_ord", nullptr, {}, BuiltinGenericKind::OrdinalValue},
    // Both operations have the same generic Pascal signature and type
    // relationship; only their ordinary RTL function bodies differ.
    {"::u_system::p_include", nullptr, {}, BuiltinGenericKind::SetMutation},
    {"::u_system::p_exclude", nullptr, {}, BuiltinGenericKind::SetMutation},
    {"::u_system::p_str", nullptr},
    {
	.cxx_name = "::u_system::p_val",
	.const_fold = nullptr,
	.generic_kind =
	    BuiltinGenericKind::ValOutput,
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
	.generic_kind =
	    BuiltinGenericKind::PointerStorage,
    },
    {
	.cxx_name = "::u_system::p_reallocmem",
	.const_fold = nullptr,
	.generic_kind =
	    BuiltinGenericKind::PointerStorage,
    },
    {"::u_system::p_freemem", nullptr},
    {"::u_system::p_rewrite", nullptr},
    {"::u_system::p_reset", nullptr},
    {"::u_system::p_close", nullptr},
    {"::u_system::p_seek", nullptr},
    {"::u_system::p_filepos", nullptr},
    {"::u_system::p_filesize", nullptr},
    {"::u_system::p_eof", nullptr},
    {"::u_system::p_truncate", nullptr},
    {"::u_system::p_ioresult", nullptr},
    {"::u_system::p_blockread", nullptr},
    {"::u_system::p_blockwrite", nullptr},
    {"::u_system::p_halt", nullptr},
    {"::u_system::p_runerror", nullptr},
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
    },
    {
	.cxx_name = "::u_system::p_writeln",
	.const_fold = nullptr,
	.syntax_kind = BuiltinSyntaxKind::WriteLn,
    },
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
	.generic_kind =
	    BuiltinGenericKind::SequenceLength,
    },
    {"::u_system::p_index", nullptr},
    {"::u_system::m_unchecked_index", nullptr},
    {"::u_system::tpcc_index_write", nullptr},
    {"::u_system::p_chr", fold_chr},
    {"::u_system::p_fillchar", nullptr},
    {"::u_system::p_move", nullptr},
    {"::u_system::p_comparebyte", nullptr},
    {"::u_system::p_comparechar", nullptr},
    {"::u_system::p_assigned", nullptr, {}, BuiltinGenericKind::Assigned},
    {"::u_system::o_trunc", fold_trunc},
    {"::u_system::o_round", fold_round},
    {"::u_system::p_frac", fold_frac},
    {"::u_system::p_sqrt", fold_sqrt},
    {"::u_system::p_exp", fold_exp},
    {"::u_system::p_ln", fold_ln},
    {"::u_system::p_pos", fold_pos},
    {"::u_system::p_copy", nullptr},
    {"::u_system::p_delete", nullptr},
    {"::u_system::p_insert", nullptr},
    // TODO: Delphi has operators "explicit", "implicit".

    {"::u_system::o_bitwiseand", nullptr},
    {"::u_system::o_bitwiseor", nullptr},
    {"::u_system::o_bitwisexor", nullptr},

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
    {"::u_system::o_implicit", nullptr},
    // Old-style file Assign is an ordinary procedure, not an implicit
    // conversion despite sharing the Pascal spelling "assign".
    {"::u_system::p_assign", nullptr},
    {"::u_system::o_modulus", fold_modulus},
    {"::u_system::o_leftshift", nullptr},
    {"::u_system::o_rightshift", nullptr},

    {"::u_system::o_lessthan", nullptr},
    {"::u_system::o_lessthanorequal", nullptr},
    {"::u_system::o_equal", nullptr},
    {"::u_system::o_greaterthan", nullptr},
    {"::u_system::o_greaterthanorequal", nullptr},
    {"::u_system::o_in", nullptr, {}, BuiltinGenericKind::SetMembership},
    {"::u_system::p_supports", nullptr},

    {"::u_system::t_boolean::p_true", nullptr},
    {"::u_system::t_boolean::p_false", nullptr},
};

// These descriptors belong only to the generic root-frame fallbacks below.
// They are deliberately not in k_builtins: concrete System arithmetic
// declarations use the same C++ operation names but have complete Pascal
// signatures and must not be mistaken for omitted-type generic declarations.
static const BuiltinDesc k_checked_inc_fallback{
    "::u_system::o_inc", nullptr, {}, BuiltinGenericKind::UnaryOrdinalOrPointerStep};
static const BuiltinDesc k_unchecked_inc_fallback{
    "::u_system::o_unchecked_inc", nullptr, {}, BuiltinGenericKind::UnaryOrdinalOrPointerStep};
static const BuiltinDesc k_checked_dec_fallback{
    "::u_system::o_dec", nullptr, {}, BuiltinGenericKind::UnaryOrdinalOrPointerStep};
static const BuiltinDesc k_unchecked_dec_fallback{
    "::u_system::o_unchecked_dec", nullptr, {}, BuiltinGenericKind::UnaryOrdinalOrPointerStep};
static const BuiltinDesc k_checked_add_fallback{
    "::u_system::o_add", nullptr, {}, BuiltinGenericKind::EnumOrPointerStep};
static const BuiltinDesc k_unchecked_add_fallback{
    "::u_system::o_unchecked_add", nullptr, {}, BuiltinGenericKind::EnumOrPointerStep};
static const BuiltinDesc k_checked_subtract_fallback{
    "::u_system::o_subtract", nullptr, {}, BuiltinGenericKind::EnumOrPointerStep};
static const BuiltinDesc k_unchecked_subtract_fallback{
    "::u_system::o_unchecked_subtract", nullptr, {}, BuiltinGenericKind::EnumOrPointerStep};
static const BuiltinDesc k_checked_pointer_difference_fallback{
    "::u_system::o_subtract", nullptr, {}, BuiltinGenericKind::PointerDifference};
static const BuiltinDesc k_unchecked_pointer_difference_fallback{
    "::u_system::o_unchecked_subtract", nullptr, {}, BuiltinGenericKind::PointerDifference};
static const BuiltinDesc k_checked_set_union_fallback{
    "::u_system::o_add", nullptr, {}, BuiltinGenericKind::SetUnionOrDifference};
static const BuiltinDesc k_unchecked_set_union_fallback{
    "::u_system::o_unchecked_add", nullptr, {}, BuiltinGenericKind::SetUnionOrDifference};
static const BuiltinDesc k_checked_set_difference_fallback{
    "::u_system::o_subtract", nullptr, {}, BuiltinGenericKind::SetUnionOrDifference};
static const BuiltinDesc k_unchecked_set_difference_fallback{
    "::u_system::o_unchecked_subtract", nullptr, {}, BuiltinGenericKind::SetUnionOrDifference};

Type* lookup_builtin_type(std::string cxx_name) {
	if (cxx_name ==
	    "::u_system::t_tmethod")
		return tmethod_type();
	for (auto t : k_all_intrinsics) {
		if (auto q = dynamic_cast<IntrinsicType*>(t)) {
			if (q->cxx_name == cxx_name) {
				return t;
			}
		} else if (auto q = dynamic_cast<ShortStringType*>(t)) {
			if (q->capacity == 255 &&
			    cxx_name == "::u_system::t_shortstring<255>") {
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
			if (q->is_untyped() &&
			    q->cxx_name == cxx_name) {
				return q;
			}
		}
	}
	return nullptr;
}

const BuiltinDesc* lookup_builtin_desc(std::string_view cxx_name) {
	for (auto& b : k_builtins) {
		if (b.cxx_name == cxx_name)
			return &b;
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

		// Inc/Dec have one exact operation for every ordinal and pointer type,
		// and their distance forms need the corresponding otherwise-infinite
		// Add/Subtract family. Pascal currently has no generic declaration
		// syntax capable of spelling those T -> T and (T, Integer) -> T
		// contracts. Represent only that missing declaration relation here:
		// these are ordinary Procedure values in the ordinary root Frame, and
		// their omitted first formals use the existing Generic match rank so a
		// typed System or user declaration always wins without a secondary
		// resolver.
		auto register_step =
		    [&ff](OperatorInvocation invocation,
			  std::string_view spelling,
			  std::size_t arity,
			  bool checked,
			  const BuiltinDesc* descriptor) {
			    auto identifier =
				operator_invocation_identifier(
				    invocation, spelling, arity,
				    checked, false);
			    assert(identifier);
			    std::vector<Parameter> formals;
			    formals.emplace_back(
				"value", "p_value",
				unknown_type(),
				ParamMode::Value, nullptr);
			    if (arity == 2)
				    formals.emplace_back(
					"amount", "p_amount",
					unknown_type(),
					ParamMode::Value,
					nullptr);
			    auto routine_type =
				new RoutineType(
				    SourceLocation::builtin(),
				    std::move(formals),
				    unknown_type(), ROUTINE);
			    auto procedure =
				new Procedure(
				    std::string(
					descriptor->cxx_name),
				    std::string(*identifier),
				    routine_type, true);
			    procedure->builtin_desc =
				descriptor;
			    procedure->is_external = true;
			    procedure->has_body = true;
			    auto registration =
				ff.register_callable(
				    std::string(*identifier),
				    procedure);
			    assert(
				registration.kind ==
				CallableRegistration::Kind::
				    Added);
		    };
		register_step(
		    OperatorInvocation::MutatingUnary,
		    "inc", 1, true,
		    &k_checked_inc_fallback);
		register_step(
		    OperatorInvocation::MutatingUnary,
		    "inc", 1, false,
		    &k_unchecked_inc_fallback);
		register_step(
		    OperatorInvocation::MutatingUnary,
		    "dec", 1, true,
		    &k_checked_dec_fallback);
		register_step(
		    OperatorInvocation::MutatingUnary,
		    "dec", 1, false,
		    &k_unchecked_dec_fallback);
		register_step(
		    OperatorInvocation::BinaryToken,
		    "+", 2, true,
		    &k_checked_add_fallback);
		register_step(
		    OperatorInvocation::BinaryToken,
		    "+", 2, false,
		    &k_unchecked_add_fallback);
		register_step(
		    OperatorInvocation::BinaryToken,
		    "-", 2, true,
		    &k_checked_subtract_fallback);
		register_step(
		    OperatorInvocation::BinaryToken,
		    "-", 2, false,
		    &k_unchecked_subtract_fallback);

		// Pascal cannot declare `(set of T, set of T) -> set of T` without
		// generic routine syntax. A set-of-unknown placeholder gives these
		// candidates distinct ordinary overload signatures; their descriptor
		// later contextualizes bracket literals and restores one concrete set
		// type without adding another lookup path.
		auto generic_set =
		    new FixedSetType(
			SourceLocation::builtin(),
			unknown_type());
		struct SetOperation {
			std::string_view spelling;
			bool checked;
			const BuiltinDesc* descriptor;
		};
		for (const SetOperation& operation :
		     std::array{
			 SetOperation{
			     "+", true,
			     &k_checked_set_union_fallback},
			 SetOperation{
			     "+", false,
			     &k_unchecked_set_union_fallback},
			 SetOperation{
			     "-", true,
			     &k_checked_set_difference_fallback},
			 SetOperation{
			     "-", false,
			     &k_unchecked_set_difference_fallback},
		     }) {
			auto identifier =
			    operator_invocation_identifier(
				OperatorInvocation::BinaryToken,
				operation.spelling, 2,
				operation.checked, false);
			assert(identifier);
			std::vector<Parameter> formals;
			formals.emplace_back(
			    "first", "p_first",
			    generic_set,
			    ParamMode::Const, nullptr);
			formals.emplace_back(
			    "second", "p_second",
			    generic_set,
			    ParamMode::Const, nullptr);
			auto routine_type =
			    new RoutineType(
				SourceLocation::builtin(),
				std::move(formals),
				unknown_type(), ROUTINE);
			auto procedure =
			    new Procedure(
				std::string(
				    operation.descriptor
					->cxx_name),
				std::string(*identifier),
				routine_type, true);
			procedure->builtin_desc =
			    operation.descriptor;
			procedure->is_external = true;
			procedure->has_body = true;
			auto registration =
			    ff.register_callable(
				std::string(*identifier),
				procedure);
			assert(
			    registration.kind ==
			    CallableRegistration::Kind::
				Added);
		}

		// Pointer subtraction is a second ordinary overload, not the
		// pointer-minus-integer step above. Pascal cannot quantify one pointee
		// type across both operands, while spelling `(Pointer, Pointer)` in
		// System would erase the element size before the RTL call. Give the
		// root declaration a distinct ordinary signature and let its
		// PointerDifference descriptor validate/preserve the actual ^T pair.
		for (auto [checked, descriptor] :
		     std::array{
			 std::pair{
			     true,
			     &k_checked_pointer_difference_fallback},
			 std::pair{
			     false,
			     &k_unchecked_pointer_difference_fallback},
		     }) {
			auto identifier =
			    operator_invocation_identifier(
				OperatorInvocation::BinaryToken,
				"-", 2, checked, false);
			assert(identifier);
			std::vector<Parameter> formals;
			formals.emplace_back(
			    "first", "p_first",
			    pointer_type(),
			    ParamMode::Value, nullptr);
			formals.emplace_back(
			    "second", "p_second",
			    pointer_type(),
			    ParamMode::Value, nullptr);
			auto routine_type =
			    new RoutineType(
				SourceLocation::builtin(),
				std::move(formals),
				ptrint_type(), ROUTINE);
			auto procedure =
			    new Procedure(
				std::string(
				    descriptor->cxx_name),
				std::string(*identifier),
				routine_type, true);
			procedure->builtin_desc =
			    descriptor;
			procedure->is_external = true;
			procedure->has_body = true;
			auto registration =
			    ff.register_callable(
				std::string(*identifier),
				procedure);
			assert(
			    registration.kind ==
			    CallableRegistration::Kind::
				Added);
		}
		return ff;
	}();
	return f;
}

#include "diagnostic.h"

const char* IntrinsicType::diagnostic_kind() const { return "intrinsic"; }
void IntrinsicType::collect_diagnostic_edges(ErrorLetContext*) const {}
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

const char* Builtin::diagnostic_kind() const { return "builtin"; }
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
