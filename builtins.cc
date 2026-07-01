#include <string>
#include "builtins.h"
#include "cst.h"
#include "frame.h"

IntrinsicType::IntrinsicType(std::string_view pas_name, std::string_view rtl_name)
	: pas_name(pas_name), rtl_name(rtl_name) {}

Builtin::Builtin(const BuiltinDesc* desc) : desc(desc) {}

// Named intrinsic type instances at namespace scope. Their addresses are
// stable and can be referenced from builtin descriptor lambdas without
// touching root_frame() during its own initialization.
// Integer rows are ordered narrowest -> widest; the ordering is what
// common_arith_type / conversion_cost use to compute widening.
namespace {
IntrinsicType k_byte       ("byte",        "pas::t_byte");
IntrinsicType k_shortint   ("shortint",    "pas::t_shortint");
IntrinsicType k_word       ("word",        "pas::t_word");
IntrinsicType k_smallint   ("smallint",    "pas::t_smallint");
IntrinsicType k_cardinal   ("cardinal",    "pas::t_cardinal");
IntrinsicType k_integer    ("integer",     "pas::t_integer");
IntrinsicType k_longint    ("longint",     "pas::t_longint");
IntrinsicType k_boolean    ("boolean",     "pas::t_boolean");
IntrinsicType k_char       ("char",        "pas::t_char");
IntrinsicType k_shortstring("shortstring", "pas::t_shortstring");

IntrinsicType* const k_all_intrinsics[] = {
	&k_byte, &k_shortint, &k_word, &k_smallint, &k_cardinal,
	&k_integer, &k_longint, &k_boolean, &k_char, &k_shortstring,
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

// Pascal-visible builtin procedures/functions. To add one: append a row
// AND implement `pas::p_<name>` in rtl.h. Linker enforces the rtl.h side.
// TODO: const_fold is nullptr for every row; wire compile-time folding
// rules for the ones that admit them (Ord on a Constant, at minimum).
static const std::array<BuiltinDesc, 3> k_builtins{{
	{"ord", "pas::p_ord", []() -> Type* { return &k_integer; }, nullptr},
	{"inc", "pas::p_inc", []() -> Type* { return &unit_type(); }, nullptr},
	{"dec", "pas::p_dec", []() -> Type* { return &unit_type(); }, nullptr},
}};

// Integer widening rank; -1 for non-integer types.
static int integer_widening_rank(Type* ty) {
	auto it = dynamic_cast<IntrinsicType*>(ty);
	if (!it) return -1;
	std::string_view n = it->pas_name;
	if (n == "byte")     return 0;
	if (n == "shortint") return 1;
	if (n == "word")     return 2;
	if (n == "smallint") return 3;
	if (n == "cardinal") return 4;
	if (n == "integer")  return 5;
	if (n == "longint")  return 6;
	return -1;
}

Type* common_arith_type(Type* a, Type* b) {
	if (!a || !b) return nullptr;
	if (a == b) return a;
	if (a == &untyped_integer_type()) return b;
	if (b == &untyped_integer_type()) return a;
	int ra = integer_widening_rank(a), rb = integer_widening_rank(b);
	if (ra < 0 || rb < 0) return nullptr;
	return (ra >= rb) ? a : b;
}

int conversion_cost(Type* from, Type* to) {
	if (!from || !to) return -1;
	if (from == to) return 0;
	if (from == &untyped_integer_type()) return 0;   // literal adapts to any int
	int rfrom = integer_widening_rank(from), rto = integer_widening_rank(to);
	if (rfrom >= 0 && rto >= 0 && rto >= rfrom) return 1;
	return -1;
}

const Frame& root_frame() {
	static const Frame f = []() {
		Frame ff(nullptr);
		for (IntrinsicType* t : k_all_intrinsics) {
			ff.register_type(std::string(t->pas_name), t);
		}
		for (auto& b : k_builtins) {
			auto bi = new Builtin(&b);
			bi->ty = b.build_type();   // Node::ty carries the return type
			ff.register_variable(std::string(b.pas_name), bi, bi->ty);
		}
		return ff;
	}();
	return f;
}
