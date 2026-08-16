#include "evaluator.h"
#include "builtins.h"
#include "cst.h"
#include "numeric_constants.h"
#include "types.h"
#include <string>

std::optional<FoldedOrdinal> folded_ordinal_value(Node* node) {
	if (!node) {
		return std::nullopt;
	}
	if (auto integer = dynamic_cast<Integer*>(node)) {
		return FoldedOrdinal{
		    node,
		    integer->ty,
		    ordinal_value(integer->negative, integer->value),
		};
	}
	if (auto member = dynamic_cast<EnumMemberRef*>(node)) {
		return FoldedOrdinal{
		    node,
		    member->ty,
		    ordinal_value(member->value),
		};
	}
	if (auto character = dynamic_cast<String*>(node);
	    character && character->ty == char_type() &&
	    character->value.size() == 1) {
		return FoldedOrdinal{
		    node,
		    character->ty,
		    ordinal_value(
		        false,
		        static_cast<unsigned char>(
		            character->value.front())),
		};
	}
	return std::nullopt;
}

static ConstEvalResult integer_result(uint64_t magnitude, bool negative, Type* ty) {
	OrdinalBounds b;
	if (!integer_bounds(ty, &b)) {
		return ConstEvalResult::error("constant integer conversion to non-integer type");
	}
	if (negative) {
		if (!b.signed_type || magnitude > b.min_magnitude) {
			return ConstEvalResult::error("integer constant out of range for target type");
		}
	} else if (magnitude > b.max_positive) {
		return ConstEvalResult::error("integer constant out of range for target type");
	}
	return ConstEvalResult::success(new Integer(magnitude, ty, negative));
}

ConstEvalResult const_convert_integer(uint64_t magnitude, bool negative, Type*, Type* to_ty) {
	if (is_real_semantic_type(to_ty)) {
		DecimalOrigin origin;
		origin.negative = negative;
		origin.digits = std::to_string(magnitude);
		origin.exponent10 = 0;
		RealMaterialization converted = materialize_decimal_origin(origin, to_ty);
		if (converted.kind == RealMaterializationKind::OutOfRange) {
			return ConstEvalResult::error("integer constant out of range for real target type");
		}
		if (converted.kind != RealMaterializationKind::InvalidTarget) {
			return ConstEvalResult::success(new Real(converted.value, to_ty));
		}
	}
	return integer_result(magnitude, negative, to_ty);
}

ConstEvalResult const_convert_string(const std::string& value, Type* to_ty) {
	if (auto target = dynamic_cast<ShortStringType*>(to_ty)) {
		return ConstEvalResult::success(new String(value.substr(0, target->capacity), to_ty));
	} else if (to_ty == ansistring_type()) {
		return ConstEvalResult::success(new String(value, to_ty));
	}
	return ConstEvalResult::error("constant string conversion has a non-string target");
}

ConstEvalResult const_explicit_ordinal_cast(uint64_t magnitude, bool negative, Type* to_ty) {
	Type* carrier = to_ty;
	while (auto range = dynamic_cast<SubrangeType*>(carrier)) {
		carrier = range->base_type;
	}

	unsigned bits = 0;
	bool signed_target = false;
	OrdinalBounds bounds;
	if (integer_bounds(carrier, &bounds)) {
		signed_target = bounds.signed_type;
		uint64_t high_bit = bounds.signed_type ? bounds.min_magnitude : bounds.max_positive;
		do {
			++bits;
			high_bit >>= 1;
		} while (high_bit != 0);
	} else if (carrier == char_type() || carrier == widechar_type()) {
		bits = carrier == char_type() ? 8 : 16;
	} else if (auto enumeration = dynamic_cast<EnumType*>(carrier)) {
		bits = enumeration->carrier_bits;
		signed_target = enumeration->carrier_signed;
	} else {
		return ConstEvalResult::error("explicit ordinal cast has a non-ordinal target");
	}

	uint64_t raw = negative ? uint64_t{0} - magnitude : magnitude;
	const uint64_t mask = bits == 64 ? UINT64_MAX : (uint64_t{1} << bits) - 1;
	raw &= mask;
	if (auto enumeration = dynamic_cast<EnumType*>(carrier)) {
		// Succ/Pred and explicit casts on enum carriers land here. The
		// resulting ordinal selects a defined member when one exists, so
		// downstream consumers (subrange bounds, designators) see the same
		// shape that Low/High produce. Values that fall in a declaration gap
		// keep the Integer-with-enum-type fallback rather than fabricating a
		// name.
		int64_t stepped = static_cast<int64_t>(raw);
		if (signed_target && (raw & (uint64_t{1} << (bits - 1)))) {
			stepped = -static_cast<int64_t>(((~raw) & mask) + 1);
		}
		if (const auto* member =
		        enumeration->member_for_value(stepped)) {
			return ConstEvalResult::success(
			    new EnumMemberRef(
				member->cxx_name,
				member->value, to_ty));
		}
	}
	if (signed_target && (raw & (uint64_t{1} << (bits - 1)))) {
		uint64_t signed_magnitude = ((~raw) & mask) + 1;
		return ConstEvalResult::success(new Integer(signed_magnitude, to_ty, true));
	}
	return ConstEvalResult::success(new Integer(raw, to_ty));
}

ConstEvalResult const_eval_type_bound(TypeBoundKind kind, Type* ty) {
	if (auto a = dynamic_cast<FixedArrayType*>(ty)) {
		// Low/High(array-type) query the declared index domain. The array
		// remains the semantic operand, but its value and result type come
		// from the bounds type rather than from the element or array carrier.
		return const_eval_type_bound(kind, a->bounds);
	} else if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		ConstEvalContext ctx;
		return (kind == TypeBoundKind::Low ? s->lower_bound : s->upper_bound)->const_eval(ctx);
	} else if (auto e = dynamic_cast<EnumType*>(ty)) {
		const auto* member = kind == TypeBoundKind::Low ? e->min_member() : e->max_member();
		if (!member) {
			return ConstEvalResult::error("low/high of empty enum type");
		}
		return ConstEvalResult::success(new EnumMemberRef(member->cxx_name, member->value, ty));
	}
	OrdinalBounds b;
	if (!intrinsic_ordinal_bounds(ty, &b)) {
		return ConstEvalResult::error("low/high of unsupported type");
	}
	if (kind == TypeBoundKind::Low) {
		if (b.signed_type) {
			return ConstEvalResult::success(new Integer(b.min_magnitude, ty, true));
		}
		return ConstEvalResult::success(new Integer(0, ty));
	}
	return ConstEvalResult::success(new Integer(b.max_positive, ty));
}
