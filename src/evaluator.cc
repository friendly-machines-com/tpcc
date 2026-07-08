#include "evaluator.h"
#include "builtins.h"
#include "cst.h"
#include "types.h"
#include <string>

struct IntegerBounds {
	bool signed_type;
	uint64_t min_magnitude; // only meaningful for signed_type: magnitude of minimum negative value
	uint64_t max_positive;
};

static bool integer_bounds(Type* ty, IntegerBounds* out) {
	if (ty == byte_type()) { *out = {false, 0, UINT8_MAX}; return true; }
	if (ty == shortint_type()) { *out = {true, 128, 127}; return true; }
	if (ty == word_type()) { *out = {false, 0, UINT16_MAX}; return true; }
	if (ty == smallint_type()) { *out = {true, 32768, 32767}; return true; }
	if (ty == cardinal_type()) { *out = {false, 0, UINT32_MAX}; return true; }
	if (ty == integer_type() || ty == longint_type()) { *out = {true, 2147483648ull, 2147483647ull}; return true; }
	if (ty == qword_type()) { *out = {false, 0, UINT64_MAX}; return true; }
	if (ty == int64_type()) { *out = {true, 9223372036854775808ull, 9223372036854775807ull}; return true; }
	return false;
}

static ConstEvalResult integer_result(uint64_t magnitude, bool negative, Type* ty) {
	IntegerBounds b;
	if (!integer_bounds(ty, &b))
		return ConstEvalResult::error("constant integer conversion to non-integer type");
	if (negative) {
		if (!b.signed_type || magnitude > b.min_magnitude)
			return ConstEvalResult::error("integer constant out of range for target type");
	} else if (magnitude > b.max_positive) {
		return ConstEvalResult::error("integer constant out of range for target type");
	}
	return ConstEvalResult::success(new Integer(magnitude, ty, negative));
}

ConstEvalResult const_convert_integer(uint64_t magnitude, bool negative, Type*, Type* to_ty) {
	return integer_result(magnitude, negative, to_ty);
}

ConstEvalResult const_eval_type_bound(TypeBoundKind kind, Type* ty) {
	IntegerBounds b;
	if (!integer_bounds(ty, &b))
		return ConstEvalResult::error("low/high of unsupported type");
	if (kind == TypeBoundKind::Low) {
		if (b.signed_type)
			return ConstEvalResult::success(new Integer(b.min_magnitude, ty, true));
		return ConstEvalResult::success(new Integer(0, ty));
	}
	return ConstEvalResult::success(new Integer(b.max_positive, ty));
}

static const Integer* as_integer(Node* n) { return dynamic_cast<const Integer*>(n); }

static ConstEvalResult fold_unary_minus(Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !as_integer(args[0]))
		return ConstEvalResult::not_constant();
	auto i = as_integer(args[0]);
	return integer_result(i->value, !i->negative && i->value != 0, result_ty);
}

static ConstEvalResult fold_unary_plus(Type* result_ty, const std::vector<Node*>& args) {
	if (args.size() != 1 || !as_integer(args[0]))
		return ConstEvalResult::not_constant();
	auto i = as_integer(args[0]);
	return integer_result(i->value, i->negative, result_ty);
}

static bool add_u64(uint64_t a, uint64_t b, uint64_t* out) {
	*out = a + b;
	return *out >= a;
}

static ConstEvalResult fold_binary(Type* result_ty, const std::vector<Node*>& args, char op) {
	if (args.size() != 2 || !as_integer(args[0]) || !as_integer(args[1]))
		return ConstEvalResult::not_constant();
	auto a = as_integer(args[0]);
	auto b = as_integer(args[1]);
	bool neg = false;
	uint64_t mag = 0;
	if (op == '+') {
		if (a->negative == b->negative) {
			if (!add_u64(a->value, b->value, &mag))
				return ConstEvalResult::error("integer constant overflow");
			neg = a->negative;
		} else if (a->value >= b->value) {
			mag = a->value - b->value;
			neg = a->negative;
		} else {
			mag = b->value - a->value;
			neg = b->negative;
		}
	} else if (op == '-') {
		Integer tmp(b->value, b->ty, !b->negative && b->value != 0);
		std::vector<Node*> v{const_cast<Integer*>(a), &tmp};
		return fold_binary(result_ty, v, '+');
	} else {
		return ConstEvalResult::not_constant();
	}
	return integer_result(mag, neg && mag != 0, result_ty);
}

ConstEvalResult const_eval_builtin_call(ConstEvalContext&, Callable* callee, const std::vector<Node*>& args) {
	if (!callee)
		return ConstEvalResult::not_constant();
	std::string name = callee->cxx_name;
	Type* result_ty = callee->ty ? callee->ty->return_type : nullptr;
	if (name == "pas::p_negative") return fold_unary_minus(result_ty, args);
	if (name == "pas::p_positive") return fold_unary_plus(result_ty, args);
	if (name == "pas::p_add") return fold_binary(result_ty, args, '+');
	if (name == "pas::p_subtract") return fold_binary(result_ty, args, '-');
	return ConstEvalResult::not_constant();
}
