#pragma once
#include <string>
#include <cstdint>
#include <vector>

class Node;
class Type;
class Callable;
class Builtin;
enum class TypeBoundKind;

struct ConstEvalContext {
};

struct ConstEvalResult {
	enum class Kind { Success, NotConstant, Error } kind = Kind::NotConstant;
	Node* node = nullptr;
	std::string message;

	static ConstEvalResult success(Node* node) { ConstEvalResult r; r.kind = Kind::Success; r.node = node; return r; }
	static ConstEvalResult not_constant() { return ConstEvalResult{}; }
	static ConstEvalResult error(std::string message) { ConstEvalResult r; r.kind = Kind::Error; r.message = std::move(message); return r; }
	bool ok() const { return kind == Kind::Success; }
};

ConstEvalResult const_convert_integer(uint64_t magnitude, bool negative, Type* from_ty, Type* to_ty);
ConstEvalResult const_eval_type_bound(TypeBoundKind kind, Type* ty);
