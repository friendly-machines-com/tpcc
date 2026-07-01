#include <string>
#include "builtins.h"
#include "cst.h"
#include "frame.h"

IntrinsicType::IntrinsicType(std::string_view pas_name, std::string_view rtl_name)
	: pas_name(pas_name), rtl_name(rtl_name) {}

Builtin::Builtin(const BuiltinDesc* desc) : desc(desc) {}

// Pascal-visible intrinsic types. To add one: append a row AND add the
// corresponding `using t_<name> = ...` in rtl.h. Linker enforces the
// rtl.h side if any emitted code uses the type.
static constexpr std::array<IntrinsicTypeDesc, 5> kIntrinsicTypes{{
	{"integer",     "pas::t_integer"},
	{"longint",     "pas::t_longint"},
	{"boolean",     "pas::t_boolean"},
	{"char",        "pas::t_char"},
	{"shortstring", "pas::t_shortstring"},
}};

// Pascal-visible builtin procedures/functions. To add one: append a row
// AND implement `pas::p_<name>` in rtl.h. Linker enforces the rtl.h side.
// TODO: build_type currently returns nullptr for every row; introduce a
// ProcedureType class so entries can carry real signatures.
// TODO: const_fold is nullptr for every row; wire compile-time folding
// rules for the ones that admit them (Ord on a Constant, at minimum).
static constexpr std::array<BuiltinDesc, 3> kBuiltins{{
	{"ord", "pas::p_ord", []() -> Type* { return nullptr; }, nullptr},
	{"inc", "pas::p_inc", []() -> Type* { return nullptr; }, nullptr},
	{"dec", "pas::p_dec", []() -> Type* { return nullptr; }, nullptr},
}};

const Frame& root_frame() {
	static const Frame f = []() {
		Frame ff(nullptr);
		for (auto& t : kIntrinsicTypes) {
			ff.register_type(std::string(t.pas_name),
			                 new IntrinsicType(t.pas_name, t.rtl_name));
		}
		for (auto& b : kBuiltins) {
			ff.register_variable(std::string(b.pas_name),
			                     new Builtin(&b),
			                     b.build_type());
		}
		return ff;
	}();
	return f;
}
