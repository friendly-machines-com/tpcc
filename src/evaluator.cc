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

static double integer_to_double(uint64_t magnitude, bool negative) {
	double d = static_cast<double>(magnitude);
	return negative ? -d : d;
}

ConstEvalResult const_convert_integer(uint64_t magnitude, bool negative, Type*, Type* to_ty) {
	if (to_ty == double_type())
		return ConstEvalResult::success(new Real(integer_to_double(magnitude, negative), to_ty));
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

