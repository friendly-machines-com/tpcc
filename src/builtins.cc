#include "builtins.h"
#include "cst.h"
#include "evaluator.h"
#include "frame.h"
#include "types.h"
#include <cmath>
#include <limits>
#include <string>

IntrinsicType::IntrinsicType(SourceLocation source_location,
			     std::string cxx_name,
			     std::optional<int> rank,
			     std::optional<OrdinalBounds> ordinal_bounds)
    : Type(std::move(source_location)),
      cxx_name(std::move(cxx_name)),
      rank(std::move(rank)),
      ordinal_bounds(std::move(ordinal_bounds)) {}

Builtin::Builtin(const BuiltinDesc* desc) : desc(desc) {}

// Integer rows are ordered narrowest -> widest; the ordering is what
// common_arith_type and conversion_cost use to compute widening.
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

IntrinsicType k_byte(SourceLocation::builtin(), "pas::t_byte", 0, unsigned_bounds(8));
IntrinsicType k_shortint(SourceLocation::builtin(), "pas::t_shortint", 1, signed_bounds(8));
IntrinsicType k_word(SourceLocation::builtin(), "pas::t_word", 2, unsigned_bounds(16));
IntrinsicType k_smallint(SourceLocation::builtin(), "pas::t_smallint", 3, signed_bounds(16));
IntrinsicType k_longword(SourceLocation::builtin(), "pas::t_longword", 4, unsigned_bounds(32));
IntrinsicType k_integer(SourceLocation::builtin(), "pas::t_integer", 5, signed_bounds(32));
IntrinsicType k_longint(SourceLocation::builtin(), "pas::t_longint", 6, signed_bounds(32));
IntrinsicType k_qword(SourceLocation::builtin(), "pas::t_qword", 7, unsigned_bounds(64));
IntrinsicType k_int64(SourceLocation::builtin(), "pas::t_int64", 8, signed_bounds(64));
IntrinsicType k_set(SourceLocation::builtin(), "pas::t_set", {});
IntrinsicType k_double(SourceLocation::builtin(), "pas::t_double", {}); // FIXME: Why {}
IntrinsicType k_extended(SourceLocation::builtin(), "pas::t_extended", {});
EnumType k_boolean(SourceLocation::builtin(), "pas::t_boolean", "false", "true");
IntrinsicType k_char(SourceLocation::builtin(), "pas::t_char", {}, unsigned_bounds(8));
IntrinsicType k_shortstring(SourceLocation::builtin(), "pas::t_shortstring", {});
IntrinsicType k_ansistring(SourceLocation::builtin(), "pas::t_ansistring", {});
IntrinsicType k_pointer(SourceLocation::builtin(), "pas::t_pointer", {});
IntrinsicType k_ptrint(SourceLocation::builtin(), "pas::t_ptrint", {});
IntrinsicType k_ptruint(SourceLocation::builtin(), "pas::t_ptruint", {});
IntrinsicType k_sizeint(SourceLocation::builtin(), "pas::t_sizeint", {});
IntrinsicType k_sizeuint(SourceLocation::builtin(), "pas::t_sizeuint", {});
IntrinsicType k_fixedarray(SourceLocation::builtin(), "pas::t_fixedarray", {});
IntrinsicType k_unknown(SourceLocation::builtin(), "pas::unknown_type", {});
#if 0
// Note: I don't think it's useful to have actual user-visible interfaces implemented on the metaclass.
//InterfaceType k_m_iobject("pas::m_iobject", new Frame(nullptr), std::vector<InterfaceType*>());

// Populate m_iobject's frame with the metaclass methods Pascal code can
// dispatch through a TClass value (e.g. `someTClassValue.ClassName`). Each
// maps to a C++ virtual method on pas::m_iobject (rtl.h); each class's
// compiler-generated m_meta (emit.cc) provides the override. Without these
// in the frame, lookups through a TClass value find nothing -- the
// metaclass methods exist only at the C++ level.
//
// The IIFE assigned to k_m_iobject_methods_initialized runs at static-init
// time. Per C++20 [basic.start.dynamic], ordered dynamic init of non-local
// variables within a single TU proceeds in textual order -- so k_m_iobject
// (declared above) is already constructed when the IIFE runs, and
// &k_m_iobject is safe to read and pass as owner_class.
const bool k_m_iobject_methods_initialized = []() {
	auto add = [](const char* pas_name, const char* cxx_name,
	              std::vector<Parameter> formals, Type* ret_ty) {
		auto sig = new RoutineType(SourceLocation::builtin(), std::move(formals), ret_ty, ROUTINE);
		auto m = new Method(cxx_name, pas_name, sig, /*has_overload_directive=*/false,
		                    &k_m_iobject, Method::VirtualKind::None);
		m->ty = sig;
		m->has_body = true;
		k_m_iobject.children->register_callable(pas_name, m);
	};
	add("classname", "p_classname", {}, &k_shortstring);
	add("inheritsfrom", "p_inheritsfrom",
	    {Parameter{"klass", "p_klass", &k_m_iobject, ParamMode::Value, nullptr}},
	    &k_boolean);
	add("classparent", "p_classparent", {}, &k_m_iobject);
	return true;
}();
#endif

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
    &k_double,
    &k_extended,
    &k_boolean,
    &k_char,
    &k_shortstring,
    &k_ansistring,
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
Type* qword_type() { return &k_qword; }
Type* int64_type() { return &k_int64; }
Type* boolean_type() { return &k_boolean; }
Type* char_type() { return &k_char; }
Type* shortstring_type() { return &k_shortstring; }
Type* ansistring_type() { return &k_ansistring; }
Type* double_type() { return &k_double; }
Type* extended_type() { return &k_extended; }
Type* set_type() { return &k_set; }
Type* fixedarray_type() { return &k_fixedarray; }
Type* unknown_type() { return &k_unknown; }

bool intrinsic_ordinal_bounds(Type* ty, OrdinalBounds* out) {
	auto intrinsic = dynamic_cast<IntrinsicType*>(ty);
	if (!intrinsic || !intrinsic->ordinal_bounds)
		return false;
	*out = *intrinsic->ordinal_bounds;
	return true;
}

bool integer_bounds(Type* ty, OrdinalBounds* out) {
	auto intrinsic = dynamic_cast<IntrinsicType*>(ty);
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

static ConstEvalResult fold_unary_minus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0]))
		return ConstEvalResult::not_constant();
	auto i = const_integer_arg(args[0]);
	return fold_integer_result(i->value, !i->negative && i->value != 0, result_ty);
}

static ConstEvalResult fold_unary_plus(ConstEvalContext&, Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !const_integer_arg(args[0]))
		return ConstEvalResult::not_constant();
	auto i = const_integer_arg(args[0]);
	return fold_integer_result(i->value, i->negative, result_ty);
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

// Pascal-visible builtin procedures/functions. To add one: append a row
// AND implement `pas::p_<name>` in rtl.h. Linker enforces the rtl.h side.
static const BuiltinDesc k_builtins[] = {
    {"pas::p_ord", nullptr, {}, BuiltinGenericKind::OrdinalValue},
    {"pas::p_inc", nullptr, {}, BuiltinGenericKind::OrdinalMutation},
    {"pas::p_dec", nullptr, {}, BuiltinGenericKind::OrdinalMutation},
    {"pas::p_str", nullptr},
    {"pas::p_low", nullptr, TypeBoundKind::Low},
    {"pas::p_high", nullptr, TypeBoundKind::High},
    {"pas::p_setlength", nullptr},
    {"pas::p_uniquestring", nullptr},
    {
        .cxx_name = "pas::p_length",
        .const_fold = fold_length,
    },
    {"pas::p_index", nullptr},
    {"pas::p_index_write", nullptr},
    {"pas::p_char_to_shortstring", nullptr},
    {"pas::p_assigned", nullptr},
    {"pas::p_trunc", fold_trunc},
    {"pas::p_round", fold_round},
    {"pas::p_frac", fold_frac},
    {"pas::p_sqrt", fold_sqrt},
    {"pas::p_exp", fold_exp},
    {"pas::p_ln", fold_ln},
    {"pas::p_pos", fold_pos},
    {"pas::p_copy", nullptr},
    {"pas::p_delete", nullptr},
    {"pas::p_insert", nullptr},
    // TODO: Delphi has operators "explicit", "implicit".

    {"pas::p_bitwiseand", nullptr},
    {"pas::p_bitwiseor", nullptr},
    {"pas::p_bitwisexor", nullptr},

    // Delphi {"pas::p_logicalor", nullptr},
    // Delphi {"pas::p_logicaland", nullptr},
    {"pas::p_logicalnot", nullptr},
    {"pas::p_logicalxor", nullptr},

    {"pas::p_add", fold_add},
    {"pas::p_subtract", fold_subtract},
    {"pas::p_positive", fold_unary_plus},
    {"pas::p_negative", fold_unary_minus},
    {"pas::p_multiply", fold_multiply},
    {"pas::p_divide", fold_divide},
    {"pas::p_intdivide", fold_intdivide},
    {"pas::p_assign", nullptr}, // delphi doesnt have it; well it has some kind of "implicit" operator that does the same.
    {"pas::p_modulus", fold_modulus},
    {"pas::p_leftshift", nullptr},
    {"pas::p_rightshift", nullptr},

    {"pas::p_lessthan", nullptr},
    {"pas::p_lessthanorequal", nullptr},
    {"pas::p_equal", nullptr},
    {"pas::p_greaterthan", nullptr},
    {"pas::p_greaterthanorequal", nullptr},
    {"pas::p_supports", nullptr},

    {"pas::t_boolean::p_true", nullptr},
    {"pas::t_boolean::p_false", nullptr},
};

Type* lookup_builtin_type(std::string cxx_name) {
	for (auto t : k_all_intrinsics) {
		if (auto q = dynamic_cast<IntrinsicType*>(t)) {
			if (q->cxx_name == cxx_name) {
				return t;
			}
		} else if (auto q = dynamic_cast<InterfaceType*>(t)) {
			if (q->cxx_name == cxx_name) {
				return q;
			}
		} else if (auto q = dynamic_cast<EnumType*>(t)) {
			if (q->cxx_name == cxx_name) {
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
		auto p_false = new EnumMemberRef("pas::t_boolean::p_false", 0, &k_boolean);
		ff.register_variable("false", p_false, &k_boolean);
		auto p_true = new EnumMemberRef("pas::t_boolean::p_true", 1, &k_boolean);
		ff.register_variable("true", p_true, &k_boolean);
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
	// A Builtin denotes an opaque C++ overload set such as pas::p_dec, not one
	// Pascal RoutineType. Do not add Node::ty here; it is intentionally null.
}
void Builtin::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	// Builtin::desc names the C++ implementation hook. Diagnostics describe the
	// source-visible value instead; the let binding carries the Pascal name when
	// the builtin is referenced from a scope.
	out << "builtin\n";
	ctx->indent(out, indent + 1);
	out << "signature: '<builtin overload set>'";
}
