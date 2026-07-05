#include "builtins.h"
#include "cst.h"
#include "frame.h"
#include "types.h"
#include <string>

IntrinsicType::IntrinsicType(std::string cxx_name, std::optional<int> rank)
    : cxx_name(std::move(cxx_name)), rank(std::move(rank)) {}

Builtin::Builtin(const BuiltinDesc* desc) : desc(desc) {}

// Integer rows are ordered narrowest -> widest; the ordering is what
// common_arith_type and conversion_cost use to compute widening.
namespace {
IntrinsicType k_byte("pas::t_byte", 0);
IntrinsicType k_shortint("pas::t_shortint", 1);
IntrinsicType k_word("pas::t_word", 2);
IntrinsicType k_smallint("pas::t_smallint", 3);
IntrinsicType k_cardinal("pas::t_cardinal", 4);
IntrinsicType k_integer("pas::t_integer", 5);
IntrinsicType k_longint("pas::t_longint", 6);
IntrinsicType k_qword("pas::t_qword", 7); // FIXME: check archs
IntrinsicType k_int64("pas::t_int64", 8); // FIXME: check archs
IntrinsicType k_double("pas::t_double", {});
IntrinsicType k_boolean("pas::t_boolean", {});
IntrinsicType k_char("pas::t_char", {});
IntrinsicType k_shortstring("pas::t_shortstring", {});

IntrinsicType* const k_all_intrinsics[] = {
    &k_byte,
    &k_shortint,
    &k_word,
    &k_smallint,
    &k_cardinal,
    &k_integer,
    &k_longint,
    &k_qword,
    &k_int64,
    &k_double,
    &k_boolean,
    &k_char,
    &k_shortstring,
};
} // namespace

UnitType& unit_type() {
	static UnitType t;
	return t;
}

UntypedIntegerType& untyped_integer_type() {
	static UntypedIntegerType t;
	return t;
}

Type* boolean_type() { return &k_boolean; }
Type* shortstring_type() { return &k_shortstring; }

// Pascal-visible builtin procedures/functions. To add one: append a row
// AND implement `pas::p_<name>` in rtl.h. Linker enforces the rtl.h side.
// TODO: const_fold is nullptr for every row; wire compile-time folding
// rules for the ones that admit them (Ord on a Constant, at minimum).
static const std::array<BuiltinDesc, 24> k_builtins{{
	// Note: constant folder would be polymorphic.
    {"pas::p_ord", nullptr},
    {"pas::p_inc", nullptr},
    {"pas::p_dec", nullptr},

    {"pas::p_bitwiseand", nullptr},
    {"pas::p_bitwiseor", nullptr},
    {"pas::p_bitwisexor", nullptr},

    {"pas::p_logicalor", nullptr},
    {"pas::p_logicaland", nullptr},
    {"pas::p_logicalnot", nullptr},
    {"pas::p_logicalxor", nullptr},

    {"pas::p_add", nullptr},
    {"pas::p_subtract", nullptr},
    {"pas::p_multiply", nullptr},
    {"pas::p_divide", nullptr},
    {"pas::p_intdivide", nullptr},
    {"pas::p_assign", nullptr}, // delphi doesnt have it
    {"pas::p_modulus", nullptr},
    {"pas::p_leftshift", nullptr},
    {"pas::p_rightshift", nullptr},

    {"pas::p_lessthan", nullptr},
    {"pas::p_lessthanorequal", nullptr},
    {"pas::p_equal", nullptr},
    //{"pas::p_not_equal", nullptr}, // delphi doesnt have it
    {"pas::p_greaterthan", nullptr},
    {"pas::p_greaterthanorequal", nullptr},

}};

// Integer widening rank; -1 for non-integer types.
static int integer_widening_rank(Type* ty) {
	auto it = dynamic_cast<IntrinsicType*>(ty);
	if (!it)
		return -1;
	if (!it->rank)
		return -1;
	return *(it->rank);
}

Type* common_arith_type(Type* a, Type* b) {
	if (!a || !b)
		return nullptr;
	if (a == b)
		return a;
	if (a == &untyped_integer_type())
		return b;
	if (b == &untyped_integer_type())
		return a;
	int ra = integer_widening_rank(a), rb = integer_widening_rank(b);
	if (ra < 0 || rb < 0)
		return nullptr;
	return (ra >= rb) ? a : b;
}

int conversion_cost(Type* from, Type* to) {
	if (!from || !to)
		return -1;
	if (from == to)
		return 0;
	if (from == &untyped_integer_type())
		return 0; // literal adapts to any int
	int rfrom = integer_widening_rank(from), rto = integer_widening_rank(to);
	if (rfrom >= 0 && rto >= 0 && rto >= rfrom)
		return 1;
	return -1;
}

IntrinsicType* lookup_builtin_type(std::string cxx_name) {
	for (IntrinsicType* t : k_all_intrinsics) {
		if (t->cxx_name == cxx_name) {
			return t;
		}
	}
	return nullptr;
}

Builtin* create_builtin_value(std::string cxx_name) {
	for (auto& b : k_builtins) {
		if (b.rtl_name == cxx_name) {
			auto bi = new Builtin(&b);
			return bi;
		}
	}
	return nullptr;
}

const Frame& root_frame() {
	static const Frame f = []() {
		Frame ff(nullptr);
		return ff;
	}();
	return f;
}
