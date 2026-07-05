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
static const std::array<BuiltinDesc, 22> k_builtins{{
	// Note: constant folder would be polymorphic.
    {"ord", "pas::p_ord", nullptr},
    {"inc", "pas::p_inc", nullptr},
    {"dec", "pas::p_dec", nullptr},

    {"and", "pas::p_and", nullptr},
    {"or", "pas::p_or", nullptr},
    {"not", "pas::p_not", nullptr},
    {"xor", "pas::p_xor", nullptr},

    {"add", "pas::p_add", nullptr},
    {"subtract", "pas::p_subtract", nullptr},
    {"multiply", "pas::p_multiply", nullptr},
    {"divide", "pas::p_divide", nullptr},
    {"assign", "pas::p_assign", nullptr},
    {"div", "pas::p_div", nullptr},
    {"mod", "pas::p_mod", nullptr},
    {"shl", "pas::p_shl", nullptr},
    {"shr", "pas::p_shr", nullptr},

    {"less", "pas::p_less", nullptr},
    {"less_equal", "pas::p_less_equal", nullptr},
    {"equal", "pas::p_equal", nullptr},
    {"not_equal", "pas::p_not_equal", nullptr},
    {"greater", "pas::p_greater", nullptr},
    {"greater_equal", "pas::p_greater_equal", nullptr},

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

const Frame& root_frame() {
	static const Frame f = []() {
		Frame ff(nullptr);
		for (IntrinsicType* t : k_all_intrinsics) {
			// FIXME: terrible seam.
			std::string pas_name = t->cxx_name;
			if (pas_name.starts_with("pas::t_")) {
				pas_name.erase(0, std::string("pas::t_").length());
			}

			ff.register_type(std::string(pas_name), t);
		}
		for (auto& b : k_builtins) {
			auto bi = new Builtin(&b);
			auto ty = nullptr; // dummy, i.e. we are so polymorphic (not variadic)
			ff.register_variable(std::string(b.pas_name), bi, ty);
		}
		return ff;
	}();
	return f;
}
