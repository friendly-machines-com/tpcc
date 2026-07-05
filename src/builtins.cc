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
IntrinsicType k_qword("pas::t_qword", 7);
IntrinsicType k_int64("pas::t_int64", 8);
IntrinsicType k_double("pas::t_double", {});
IntrinsicType k_boolean("pas::t_boolean", {});
IntrinsicType k_char("pas::t_char", {});
IntrinsicType k_shortstring("pas::t_shortstring", {});
IntrinsicType k_pointer("pas::t_pointer", {});
IntrinsicType k_ptrint("pas::t_ptrint", {});
IntrinsicType k_ptruint("pas::t_ptruint", {});

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
    &k_pointer,
    &k_ptrint,
    &k_ptruint,
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
Type* char_type() { return &k_char; }
Type* shortstring_type() { return &k_shortstring; }

// Pascal-visible builtin procedures/functions. To add one: append a row
// AND implement `pas::p_<name>` in rtl.h. Linker enforces the rtl.h side.
// TODO: const_fold is nullptr for every row; wire compile-time folding
// rules for the ones that admit them (Ord on a Constant, at minimum).
static const std::array<BuiltinDesc, 30> k_builtins{{
    // Note: constant folder would be polymorphic.
    {"pas::p_ord", nullptr},
    {"pas::p_inc", nullptr},
    {"pas::p_dec", nullptr},
    {"pas::p_assigned", nullptr},
    // TODO: Delphi has operators "explicit", "implicit".

    {"pas::p_bitwiseand", nullptr},
    {"pas::p_bitwiseor", nullptr},
    {"pas::p_bitwisexor", nullptr},

    {"pas::p_logicalor", nullptr},
    {"pas::p_logicaland", nullptr},
    {"pas::p_logicalnot", nullptr},
    {"pas::p_logicalxor", nullptr},

    {"pas::p_add", nullptr},
    {"pas::p_subtract", nullptr},
    {"pas::p_positive", nullptr},
    {"pas::p_negative", nullptr},
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
		if (b.cxx_name == cxx_name) {
			auto bi = new Builtin(&b);
			return bi;
		}
	}
	// Fallback to anything, we will get a linker error anyway.
	fprintf(stderr, "warning: builtin '%s' doesn't have a registration.  Allowing it--but it won't constant-fold.\n", cxx_name.c_str());
	auto bs = BuiltinDesc {cxx_name, nullptr};
	auto bi = new Builtin(&bs);
	return bi;
}

const Frame& root_frame() {
	static const Frame f = []() {
		Frame ff(nullptr);
		return ff;
	}();
	return f;
}
