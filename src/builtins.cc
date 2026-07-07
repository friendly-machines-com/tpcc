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
EnumType k_boolean("pas::t_boolean", "p_false", "p_true");
IntrinsicType k_char("pas::t_char", {});
IntrinsicType k_shortstring("pas::t_shortstring", {});
IntrinsicType k_pointer("pas::t_pointer", {});
IntrinsicType k_ptrint("pas::t_ptrint", {});
IntrinsicType k_ptruint("pas::t_ptruint", {});
IntrinsicType k_sizeint("pas::t_sizeint", {});
IntrinsicType k_sizeuint("pas::t_sizeuint", {});
IntrinsicType k_unknown("pas::unknown_type", {});
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
		auto sig = new RoutineType(std::move(formals), ret_ty, ROUTINE);
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
    &k_sizeint,
    &k_sizeuint,
    &k_unknown,
//    &k_m_iobject,
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
Type* unknown_type() { return &k_unknown; }

// Pascal-visible builtin procedures/functions. To add one: append a row
// AND implement `pas::p_<name>` in rtl.h. Linker enforces the rtl.h side.
// TODO: const_fold is nullptr for every row; wire compile-time folding
// rules for the ones that admit them (Ord on a Constant, at minimum).
static const std::array<BuiltinDesc, 32> k_builtins{{
    // Note: constant folder would be polymorphic.
    {"pas::p_ord", nullptr},
    {"pas::p_inc", nullptr},
    {"pas::p_dec", nullptr},
    {"pas::p_assigned", nullptr},
    // TODO: Delphi has operators "explicit", "implicit".

    {"pas::p_bitwiseand", nullptr},
    {"pas::p_bitwiseor", nullptr},
    {"pas::p_bitwisexor", nullptr},

    // Delphi {"pas::p_logicalor", nullptr},
    // Delphi {"pas::p_logicaland", nullptr},
    {"pas::p_logicalnot", nullptr},
    {"pas::p_logicalxor", nullptr},

    {"pas::p_add", nullptr},
    {"pas::p_subtract", nullptr},
    {"pas::p_positive", nullptr},
    {"pas::p_negative", nullptr},
    {"pas::p_multiply", nullptr},
    {"pas::p_divide", nullptr},
    {"pas::p_intdivide", nullptr},
    {"pas::p_assign", nullptr}, // delphi doesnt have it; well it has some kind of "implicit" operator that does the same.
    {"pas::p_modulus", nullptr},
    {"pas::p_leftshift", nullptr},
    {"pas::p_rightshift", nullptr},

    {"pas::p_lessthan", nullptr},
    {"pas::p_lessthanorequal", nullptr},
    {"pas::p_equal", nullptr},
    //{"pas::p_not_equal", nullptr}, // delphi doesnt have it
    {"pas::p_greaterthan", nullptr},
    {"pas::p_greaterthanorequal", nullptr},
    {"pas::p_supports", nullptr},

    {"pas::t_boolean::p_true", nullptr},
    {"pas::t_boolean::p_false", nullptr},
}};

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
void IntrinsicType::print_diagnostic_definition(ErrorLetContext*, std::ostringstream& out, unsigned) const {
	out << cxx_name;
	if (rank)
		out << " rank " << *rank;
}

const char* Builtin::diagnostic_kind() const { return "builtin"; }
void Builtin::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "builtin " << desc->cxx_name << " : " << ctx->known_type_ref(ty);
}
