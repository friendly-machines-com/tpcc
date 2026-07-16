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
	if (to_ty == single_type() ||
	    to_ty == double_type() ||
	    to_ty == extended_type())
		return ConstEvalResult::success(new Real(integer_to_real(magnitude, negative), to_ty));
	return integer_result(magnitude, negative, to_ty);
}

ConstEvalResult const_explicit_ordinal_cast(
    uint64_t magnitude, bool negative,
    Type* to_ty) {
	Type* carrier = to_ty;
	while (auto range =
	           dynamic_cast<SubrangeType*>(
	               carrier))
		carrier = range->base_type;

	unsigned bits = 0;
	bool signed_target = false;
	OrdinalBounds bounds;
	if (integer_bounds(carrier, &bounds)) {
		signed_target = bounds.signed_type;
		uint64_t high_bit = bounds.signed_type
		    ? bounds.min_magnitude
		    : bounds.max_positive;
		do {
			++bits;
			high_bit >>= 1;
		} while (high_bit != 0);
	} else if (carrier == char_type()) {
		bits = 8;
	} else if (dynamic_cast<EnumType*>(
	               carrier)) {
		// Generated enums use a signed 32-bit underlying carrier.
		bits = 32;
		signed_target = true;
	} else {
		return ConstEvalResult::error(
		    "explicit ordinal cast has a non-ordinal target");
	}

	uint64_t raw = negative
	    ? uint64_t{0} - magnitude
	    : magnitude;
	const uint64_t mask = bits == 64
	    ? UINT64_MAX
	    : (uint64_t{1} << bits) - 1;
	raw &= mask;
	if (signed_target &&
	    (raw & (uint64_t{1} << (bits - 1)))) {
		uint64_t signed_magnitude =
		    ((~raw) & mask) + 1;
		return ConstEvalResult::success(
		    new Integer(
		        signed_magnitude, to_ty,
		        true));
	}
	return ConstEvalResult::success(
	    new Integer(raw, to_ty));
}

ConstEvalResult const_eval_type_bound(TypeBoundKind kind, Type* ty) {
	if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		ConstEvalContext ctx;
		return (kind == TypeBoundKind::Low ? s->lower_bound : s->upper_bound)->const_eval(ctx);
	}
	if (auto e = dynamic_cast<EnumType*>(ty)) {
		const auto* member = kind == TypeBoundKind::Low
		    ? e->min_member()
		    : e->max_member();
		if (!member)
			return ConstEvalResult::error("low/high of empty enum type");
		return ConstEvalResult::success(
		    new EnumMemberRef(
		        member->cxx_name, member->value, ty));
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
