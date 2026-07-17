#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

/** Source constructs which spell an operator use. This is parser metadata,
 * not an overload-resolution kind: a row translates the source spelling to
 * an ordinary Pascal declaration identifier, after which TPCC uses the
 * existing Frame lookup and callable matcher unchanged. */
enum class OperatorInvocation {
	ImplicitConversion,
	ExplicitConversion,
	UnaryToken,
	BinaryToken,
	NamedUnary,
	MutatingUnary,
	Lifecycle,
};

/** Conditions known at the source syntax before overload selection. Checked
 * versus unchecked is deliberately policy-neutral: arithmetic invocations
 * supply the caller's {$Q} state, while implicit conversions supply {$R}. */
enum class OperatorSelection {
	Always,
	Checked,
	Unchecked,
	Logical,
	Bitwise,
};

enum class OperatorProvenance {
	Delphi,
	TpccExtension,
	LegacyFpc,
	DelphiLifecycle,
};

struct OperatorSpec {
	std::string_view declaration_name;
	std::string_view invocation_spelling;
	std::size_t arity;
	OperatorInvocation invocation;
	OperatorSelection selection;
	std::string_view pascal_identifier;
	std::string_view cxx_name;
	bool boolean_result;
	OperatorProvenance provenance;
	bool implemented;
};

std::span<const OperatorSpec> operator_catalog();

/** All semantic rows promised by one source declaration. Legacy symbolic
 * declarations may return two rows, for example `operator +` satisfies both
 * checked and unchecked addition. */
std::vector<const OperatorSpec*> operator_declaration_specs(
    std::string_view declaration_name, std::size_t arity);

bool operator_declaration_name_known(
    std::string_view declaration_name);

/** Translate operator syntax to the canonical Pascal identifier which the
 * parser then passes to ordinary value lookup. Multiple catalog rows may
 * describe named and legacy declaration spellings for that operation, but
 * they must agree on this identifier. Declarations are registered under the
 * same identifier, and future RTTI will enumerate that declaration name. */
std::optional<std::string_view> operator_invocation_identifier(
    OperatorInvocation invocation,
    std::string_view spelling, std::size_t arity,
    bool checks_enabled, bool logical_operands);

/** C++ spelling assigned to a declaration. Every semantic row produced by one
 * declaration spelling and arity must agree on this name. */
std::optional<std::string_view> operator_declaration_cxx_name(
    std::string_view declaration_name, std::size_t arity);

std::optional<std::string_view> legacy_operator_cxx_name(
    std::string_view declaration_name);

std::string_view implicit_operator_identifier(
    bool range_checks);
std::string_view explicit_operator_identifier();
