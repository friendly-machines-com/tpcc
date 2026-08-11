#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

class Type;

/** Exact source value of one decimal real numeral.
 *
 *  value = (negative ? -1 : 1) * digits * 10^exponent10.
 *
 * `digits` is canonical: "0", or a non-zero digit followed by digits with no
 * leading or trailing zero. The sign of zero remains meaningful because
 * materialization into an IEEE type must preserve -0.0. */
struct DecimalOrigin {
	bool negative = false;
	std::string digits = "0";
	int64_t exponent10 = 0;

	bool is_zero() const {
		return digits == "0";
	}
};

std::optional<DecimalOrigin> parse_decimal_origin(std::string_view spelling, std::string* error = nullptr);
std::string decimal_origin_text(const DecimalOrigin& origin);

enum class RealMaterializationKind {
	Exact,
	Rounded,
	OutOfRange,
	InvalidTarget,
};

struct RealMaterialization {
	RealMaterializationKind kind = RealMaterializationKind::InvalidTarget;
	// A typed real is currently stored losslessly in the compiler's long
	// double container after it has been rounded to its Pascal destination.
	long double value = 0.0L;
};

/** Architecture-neutral semantic order of the real types exposed by the
 * intrinsic Pascal environment. Distinct identities are inspected through
 * their storage type. */
int real_semantic_rank(const Type* type);
bool is_real_semantic_type(const Type* type);
bool integer_domain_is_exact_in_real(const Type* integer_type, const Type* real_type);

RealMaterialization materialize_decimal_origin(const DecimalOrigin& origin, Type* target);
std::optional<long double> round_typed_real(long double value, Type* target);
bool typed_real_out_of_range(long double value, Type* target);

enum class RealBinaryOperation {
	Add,
	Subtract,
	Multiply,
	Divide,
};

/** Execute one already-selected typed real equation. Operands are values of
 * the selected formal type stored losslessly in long double; the arithmetic
 * itself is performed in that formal/result carrier and rounded there. */
std::optional<long double> eval_typed_real_binary(RealBinaryOperation operation, long double left, long double right, Type* result_type);
