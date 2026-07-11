#include "evaluator.h"
#include "builtins.h"
#include "cst.h"
#include "types.h"
#include <string>

static ConstEvalResult integer_result(uint64_t magnitude, bool negative, Type* ty) {
	OrdinalBounds b;
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

static long double integer_to_real(uint64_t magnitude, bool negative) {
	long double d = static_cast<long double>(magnitude);
	return negative ? -d : d;
}

ConstEvalResult const_convert_integer(uint64_t magnitude, bool negative, Type*, Type* to_ty) {
	if (to_ty == double_type() || to_ty == extended_type())
		return ConstEvalResult::success(new Real(integer_to_real(magnitude, negative), to_ty));
	return integer_result(magnitude, negative, to_ty);
}

ConstEvalResult const_eval_type_bound(TypeBoundKind kind, Type* ty) {
	if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		ConstEvalContext ctx;
		return (kind == TypeBoundKind::Low ? s->lower_bound : s->upper_bound)->const_eval(ctx);
	}
	if (auto e = dynamic_cast<EnumType*>(ty)) {
		if (e->members.empty())
			return ConstEvalResult::error("low/high of empty enum type");
		const auto& member = kind == TypeBoundKind::Low ? e->members.front() : e->members.back();
		return ConstEvalResult::success(new EnumMemberRef(member.cxx_name, member.value, ty));
	}
	OrdinalBounds b;
	if (!intrinsic_ordinal_bounds(ty, &b))
		return ConstEvalResult::error("low/high of unsupported type");
	if (kind == TypeBoundKind::Low) {
		if (b.signed_type)
			return ConstEvalResult::success(new Integer(b.min_magnitude, ty, true));
		return ConstEvalResult::success(new Integer(0, ty));
	}
	return ConstEvalResult::success(new Integer(b.max_positive, ty));
}
