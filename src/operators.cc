#include "operators.h"

#include <cassert>

namespace {

using I = OperatorInvocation;
using S = OperatorSelection;
using P = OperatorProvenance;

// This is TPCC's authoritative operator vocabulary. Each &op_... value is the
// declaration's canonical Pascal identifier. `operator Add` registers its
// Callable under &op_CheckedAddition in the ordinary value map; `A + B` under
// {$Q+} translates to that same identifier and passes it to ordinary Frame
// lookup and overload resolution. Future RTTI therefore sees the declaration
// identifier already stored by the compiler rather than a second operator
// registry. Keep the source spellings, Pascal identifier, and C++ ABI spelling
// in one row so declarations and expressions cannot acquire different names.
constexpr OperatorSpec k_operator_catalog[] = {
    // Conversions.
    {"implicit", ":implicit", 1, I::ImplicitConversion, S::Checked,
     "&op_CheckedImplicit", "o_implicit", false, P::Delphi, true},
    {"uncheckedimplicit", ":implicit", 1, I::ImplicitConversion,
     S::Unchecked, "&op_Implicit", "o_unchecked_implicit", false,
     P::TpccExtension, true},
    {":=", ":implicit", 1, I::ImplicitConversion, S::Checked,
     "&op_CheckedImplicit", "o_implicit", false, P::LegacyFpc, true},
    {":=", ":implicit", 1, I::ImplicitConversion, S::Unchecked,
     "&op_Implicit", "o_implicit", false, P::LegacyFpc, true},
    {"explicit", ":explicit", 1, I::ExplicitConversion, S::Always,
     "&op_Explicit", "o_explicit", false, P::Delphi, true},

    // Unary expression operators.
    {"negative", "-", 1, I::UnaryToken, S::Checked,
     "&op_CheckedUnaryNegation", "o_negative", false, P::Delphi, true},
    {"uncheckednegative", "-", 1, I::UnaryToken, S::Unchecked,
     "&op_UnaryNegation", "o_unchecked_negative", false,
     P::TpccExtension, true},
    {"-", "-", 1, I::UnaryToken, S::Checked,
     "&op_CheckedUnaryNegation", "o_operator_minus", false,
     P::LegacyFpc, true},
    {"-", "-", 1, I::UnaryToken, S::Unchecked,
     "&op_UnaryNegation", "o_operator_minus", false,
     P::LegacyFpc, true},
    {"positive", "+", 1, I::UnaryToken, S::Always,
     "&op_UnaryPlus", "o_positive", false, P::Delphi, true},
    {"+", "+", 1, I::UnaryToken, S::Always,
     "&op_UnaryPlus", "o_operator_plus", false, P::LegacyFpc, true},
    {"logicalnot", "not", 1, I::UnaryToken, S::Always,
     "&op_LogicalNot", "o_logicalnot", false, P::Delphi, true},
    {"not", "not", 1, I::UnaryToken, S::Always,
     "&op_LogicalNot", "o_not", false, P::LegacyFpc, true},

    // Unary mutation. The returned value is stored back into the source place.
    {"inc", "inc", 1, I::MutatingUnary, S::Checked,
     "&op_CheckedIncrement", "o_inc", false, P::Delphi, true},
    {"uncheckedinc", "inc", 1, I::MutatingUnary, S::Unchecked,
     "&op_Increment", "o_unchecked_inc", false, P::TpccExtension, true},
    {"dec", "dec", 1, I::MutatingUnary, S::Checked,
     "&op_CheckedDecrement", "o_dec", false, P::Delphi, true},
    {"uncheckeddec", "dec", 1, I::MutatingUnary, S::Unchecked,
     "&op_Decrement", "o_unchecked_dec", false, P::TpccExtension, true},

    // Named unary syntax. These spellings denote operators, not ordinary
    // functions with the same Pascal identifier.
    {"trunc", "trunc", 1, I::NamedUnary, S::Always,
     "&op_Trunc", "o_trunc", false, P::Delphi, true},
    {"round", "round", 1, I::NamedUnary, S::Always,
     "&op_Round", "o_round", false, P::Delphi, true},

    // Comparisons.
    {"equal", "=", 2, I::BinaryToken, S::Always,
     "&op_Equality", "o_equal", true, P::Delphi, true},
    {"=", "=", 2, I::BinaryToken, S::Always,
     "&op_Equality", "o_operator_equal", true, P::LegacyFpc, true},
    {"notequal", "<>", 2, I::BinaryToken, S::Always,
     "&op_Inequality", "o_notequal", true, P::Delphi, true},
    {"<>", "<>", 2, I::BinaryToken, S::Always,
     "&op_Inequality", "o_operator_not_equal", true, P::LegacyFpc, true},
    {"greaterthan", ">", 2, I::BinaryToken, S::Always,
     "&op_GreaterThan", "o_greaterthan", true, P::Delphi, true},
    {">", ">", 2, I::BinaryToken, S::Always,
     "&op_GreaterThan", "o_operator_greater", true, P::LegacyFpc, true},
    {"greaterthanorequal", ">=", 2, I::BinaryToken, S::Always,
     "&op_GreaterThanOrEqual", "o_greaterthanorequal", true,
     P::Delphi, true},
    {">=", ">=", 2, I::BinaryToken, S::Always,
     "&op_GreaterThanOrEqual", "o_operator_greater_equal", true,
     P::LegacyFpc, true},
    {"lessthan", "<", 2, I::BinaryToken, S::Always,
     "&op_LessThan", "o_lessthan", true, P::Delphi, true},
    {"<", "<", 2, I::BinaryToken, S::Always,
     "&op_LessThan", "o_operator_less", true, P::LegacyFpc, true},
    {"lessthanorequal", "<=", 2, I::BinaryToken, S::Always,
     "&op_LessThanOrEqual", "o_lessthanorequal", true, P::Delphi, true},
    {"<=", "<=", 2, I::BinaryToken, S::Always,
     "&op_LessThanOrEqual", "o_operator_less_equal", true,
     P::LegacyFpc, true},

    // Checked arithmetic and its legacy symbolic declarations.
    {"add", "+", 2, I::BinaryToken, S::Checked,
     "&op_CheckedAddition", "o_add", false, P::Delphi, true},
    {"uncheckedadd", "+", 2, I::BinaryToken, S::Unchecked,
     "&op_Addition", "o_unchecked_add", false, P::TpccExtension, true},
    {"+", "+", 2, I::BinaryToken, S::Checked,
     "&op_CheckedAddition", "o_operator_plus", false, P::LegacyFpc, true},
    {"+", "+", 2, I::BinaryToken, S::Unchecked,
     "&op_Addition", "o_operator_plus", false, P::LegacyFpc, true},
    {"subtract", "-", 2, I::BinaryToken, S::Checked,
     "&op_CheckedSubtraction", "o_subtract", false, P::Delphi, true},
    {"uncheckedsubtract", "-", 2, I::BinaryToken, S::Unchecked,
     "&op_Subtraction", "o_unchecked_subtract", false,
     P::TpccExtension, true},
    {"-", "-", 2, I::BinaryToken, S::Checked,
     "&op_CheckedSubtraction", "o_operator_minus", false,
     P::LegacyFpc, true},
    {"-", "-", 2, I::BinaryToken, S::Unchecked,
     "&op_Subtraction", "o_operator_minus", false, P::LegacyFpc, true},
    {"multiply", "*", 2, I::BinaryToken, S::Checked,
     "&op_CheckedMultiply", "o_multiply", false, P::Delphi, true},
    {"uncheckedmultiply", "*", 2, I::BinaryToken, S::Unchecked,
     "&op_Multiply", "o_unchecked_multiply", false,
     P::TpccExtension, true},
    {"*", "*", 2, I::BinaryToken, S::Checked,
     "&op_CheckedMultiply", "o_operator_multiply", false,
     P::LegacyFpc, true},
    {"*", "*", 2, I::BinaryToken, S::Unchecked,
     "&op_Multiply", "o_operator_multiply", false, P::LegacyFpc, true},
    {"intdivide", "div", 2, I::BinaryToken, S::Checked,
     "&op_CheckedIntDivide", "o_intdivide", false, P::Delphi, true},
    {"uncheckedintdivide", "div", 2, I::BinaryToken,
     S::Unchecked, "&op_IntDivide", "o_unchecked_intdivide",
     false, P::TpccExtension, true},
    {"div", "div", 2, I::BinaryToken, S::Checked,
     "&op_CheckedIntDivide", "o_operator_intdivide", false,
     P::LegacyFpc, true},
    {"div", "div", 2, I::BinaryToken, S::Unchecked,
     "&op_IntDivide", "o_operator_intdivide", false, P::LegacyFpc, true},

    // Arithmetic with one family in both overflow modes.
    {"divide", "/", 2, I::BinaryToken, S::Always,
     "&op_Division", "o_divide", false, P::Delphi, true},
    {"/", "/", 2, I::BinaryToken, S::Always,
     "&op_Division", "o_operator_divide", false, P::LegacyFpc, true},
    {"modulus", "mod", 2, I::BinaryToken, S::Always,
     "&op_Modulus", "o_modulus", false, P::Delphi, true},
    {"mod", "mod", 2, I::BinaryToken, S::Always,
     "&op_Modulus", "o_operator_modulus", false, P::LegacyFpc, true},
    {"leftshift", "shl", 2, I::BinaryToken, S::Always,
     "&op_LeftShift", "o_leftshift", false, P::Delphi, true},
    {"shl", "shl", 2, I::BinaryToken, S::Always,
     "&op_LeftShift", "o_shl", false, P::LegacyFpc, true},
    {"rightshift", "shr", 2, I::BinaryToken, S::Always,
     "&op_RightShift", "o_rightshift", false, P::Delphi, true},
    {"shr", "shr", 2, I::BinaryToken, S::Always,
     "&op_RightShift", "o_shr", false, P::LegacyFpc, true},
    {"**", "**", 2, I::BinaryToken, S::Always,
     "&op_Exponentiation", "o_operator_power", false, P::LegacyFpc, true},
    {"><", "><", 2, I::BinaryToken, S::Always,
     "&op_SymmetricDifference", "o_operator_symmetric_difference", false,
     P::LegacyFpc, true},

    // `and`, `or`, and `xor` select distinct Delphi families from the exact
    // operand categories before ordinary overload resolution.
    {"logicaland", "and", 2, I::BinaryToken, S::Logical,
     "&op_LogicalAnd", "o_logicaland", false, P::Delphi, true},
    {"bitwiseand", "and", 2, I::BinaryToken, S::Bitwise,
     "&op_BitwiseAnd", "o_bitwiseand", false, P::Delphi, true},
    {"and", "and", 2, I::BinaryToken, S::Logical,
     "&op_LogicalAnd", "o_and", false, P::LegacyFpc, true},
    {"and", "and", 2, I::BinaryToken, S::Bitwise,
     "&op_BitwiseAnd", "o_and", false, P::LegacyFpc, true},
    {"logicalor", "or", 2, I::BinaryToken, S::Logical,
     "&op_LogicalOr", "o_logicalor", false, P::Delphi, true},
    {"bitwiseor", "or", 2, I::BinaryToken, S::Bitwise,
     "&op_BitwiseOr", "o_bitwiseor", false, P::Delphi, true},
    {"or", "or", 2, I::BinaryToken, S::Logical,
     "&op_LogicalOr", "o_or", false, P::LegacyFpc, true},
    {"or", "or", 2, I::BinaryToken, S::Bitwise,
     "&op_BitwiseOr", "o_or", false, P::LegacyFpc, true},
    {"logicalxor", "xor", 2, I::BinaryToken, S::Logical,
     "&op_LogicalXor", "o_logicalxor", false, P::Delphi, true},
    {"bitwisexor", "xor", 2, I::BinaryToken, S::Bitwise,
     "&op_BitwiseXOR", "o_bitwisexor", false, P::Delphi, true},
    {"xor", "xor", 2, I::BinaryToken, S::Logical,
     "&op_LogicalXor", "o_xor", false, P::LegacyFpc, true},
    {"xor", "xor", 2, I::BinaryToken, S::Bitwise,
     "&op_BitwiseXOR", "o_xor", false, P::LegacyFpc, true},

    // Membership.
    {"in", "in", 2, I::BinaryToken, S::Always,
     "&op_In", "o_in", true, P::Delphi, true},

    // Modern Delphi managed-record hooks are catalogued so the accepted
    // language vocabulary is honest, but they are not expression operators
    // and require a separate whole-lifetime implementation.
    {"initialize", ":initialize", 1, I::Lifecycle, S::Always,
     "&op_Initialize", "o_initialize", false, P::DelphiLifecycle, false},
    {"finalize", ":finalize", 1, I::Lifecycle, S::Always,
     "&op_Finalize", "o_finalize", false, P::DelphiLifecycle, false},
    {"assign", ":assign", 2, I::Lifecycle, S::Always,
     "&op_Assign", "o_assign", false, P::DelphiLifecycle, false},
};

bool selection_matches(
    OperatorSelection selection,
    bool checks_enabled,
    bool logical_operands) {
	switch (selection) {
	case OperatorSelection::Always:
		return true;
	case OperatorSelection::Checked:
		return checks_enabled;
	case OperatorSelection::Unchecked:
		return !checks_enabled;
	case OperatorSelection::Logical:
		return logical_operands;
	case OperatorSelection::Bitwise:
		return !logical_operands;
	}
	return false;
}

} // namespace

std::span<const OperatorSpec> operator_catalog() {
	return k_operator_catalog;
}

std::vector<const OperatorSpec*> operator_declaration_specs(
    std::string_view declaration_name, std::size_t arity) {
	std::vector<const OperatorSpec*> result;
	for (const OperatorSpec& spec : k_operator_catalog)
		if (spec.declaration_name == declaration_name &&
		    spec.arity == arity)
			result.push_back(&spec);
	return result;
}

bool operator_declaration_name_known(
    std::string_view declaration_name) {
	for (const OperatorSpec& spec : k_operator_catalog)
		if (spec.declaration_name == declaration_name)
			return true;
	return false;
}

std::optional<std::string_view> operator_invocation_identifier(
    OperatorInvocation invocation,
    std::string_view spelling, std::size_t arity,
    bool checks_enabled, bool logical_operands) {
	std::optional<std::string_view> result;
	for (const OperatorSpec& spec : k_operator_catalog) {
		if (!spec.implemented ||
		    spec.invocation != invocation ||
		    spec.invocation_spelling != spelling ||
		    spec.arity != arity ||
		    !selection_matches(
			spec.selection, checks_enabled,
			logical_operands))
			continue;
		if (result)
			assert(*result == spec.pascal_identifier);
		else
			result = spec.pascal_identifier;
	}
	return result;
}

std::optional<std::string_view> operator_declaration_cxx_name(
    std::string_view declaration_name, std::size_t arity) {
	std::optional<std::string_view> result;
	for (const OperatorSpec* spec :
	     operator_declaration_specs(
		 declaration_name, arity)) {
		if (result)
			assert(*result == spec->cxx_name);
		else
			result = spec->cxx_name;
	}
	return result;
}

std::optional<std::string_view> legacy_operator_cxx_name(
    std::string_view declaration_name) {
	std::optional<std::string_view> result;
	for (const OperatorSpec& spec : k_operator_catalog) {
		if (spec.declaration_name != declaration_name ||
		    spec.provenance !=
			OperatorProvenance::LegacyFpc)
			continue;
		if (result)
			assert(*result == spec.cxx_name);
		else
			result = spec.cxx_name;
	}
	return result;
}

std::string_view implicit_operator_identifier(
    bool range_checks) {
	auto result = operator_invocation_identifier(
	    OperatorInvocation::ImplicitConversion,
	    ":implicit", 1, range_checks, false);
	assert(result);
	return *result;
}

std::string_view explicit_operator_identifier() {
	auto result = operator_invocation_identifier(
	    OperatorInvocation::ExplicitConversion,
	    ":explicit", 1, false, false);
	assert(result);
	return *result;
}
