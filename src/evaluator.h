#pragma once
#include "types.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Node;
class Callable;
class Builtin;
enum class TypeBoundKind;

struct ConstEvalContext {};

struct ConstEvalResult {
	enum class Kind { Success, NotConstant, Error } kind = Kind::NotConstant;
	Node* node = nullptr;
	std::string message;

	static ConstEvalResult success(Node* node) {
		ConstEvalResult r;
		r.kind = Kind::Success;
		r.node = node;
		return r;
	}

	static ConstEvalResult not_constant() {
		return ConstEvalResult{};
	}

	static ConstEvalResult error(std::string message) {
		ConstEvalResult r;
		r.kind = Kind::Error;
		r.message = std::move(message);
		return r;
	}

	bool ok() const {
		return kind == Kind::Success;
	}
};

/** A folded CST representation viewed as one typed Pascal ordinal value.
 * Integer, Char, and enumeration constants have several useful CST spellings;
 * semantic consumers use this view so those spellings cannot become different
 * Pascal value categories. This operation only reads an already-folded node:
 * it does not select a conversion or invoke constant evaluation itself. */
struct FoldedOrdinal {
	Node* representation = nullptr;
	Type* exact_type = nullptr;
	OrdinalValue value;
};

std::optional<FoldedOrdinal> folded_ordinal_value(Node* node);

ConstEvalResult const_convert_integer(uint64_t magnitude, bool negative, Type* from_ty, Type* to_ty);
ConstEvalResult const_explicit_ordinal_cast(uint64_t magnitude, bool negative, Type* to_ty);
ConstEvalResult const_convert_string(const std::string& value, Type* to_ty);
ConstEvalResult const_eval_type_bound(TypeBoundKind kind, Type* ty);
