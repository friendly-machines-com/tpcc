#include "builtins.h"
#include "types.h"
#include <cassert>

int main() {
	auto shortint_to_int64 =
	    int64_type()->value_conversion_from(
	        shortint_type());
	auto shortint_to_single =
	    single_type()->value_conversion_from(
	        shortint_type());
	auto byte_to_int64 =
	    int64_type()->value_conversion_from(
	        byte_type());
	auto integer_to_cardinal =
	    cardinal_type()->value_conversion_from(
	        integer_type());
	auto integer_to_int64 =
	    int64_type()->value_conversion_from(
	        integer_type());
	auto byte_to_shortint =
	    shortint_type()->value_conversion_from(
	        byte_type());
	auto cardinal_to_int64 =
	    int64_type()->value_conversion_from(
	        cardinal_type());
	auto double_to_single =
	    single_type()->value_conversion_from(
	        double_type());
	auto single_to_double =
	    double_type()->value_conversion_from(
	        single_type());

	assert(shortint_to_int64);
	assert(shortint_to_single);
	assert(byte_to_int64);
	assert(!integer_to_cardinal);
	assert(integer_to_int64);
	assert(!byte_to_shortint);
	assert(cardinal_to_int64);
	assert(!double_to_single);
	assert(single_to_double);
	assert(shortint_to_int64->kind ==
	       ValueConversionClass::Convert);
	assert(shortint_to_single->kind ==
	       ValueConversionClass::Convert);
	assert(shortint_to_int64->distance <
	       shortint_to_single->distance);

	PointerType pointer_a(
	    SourceLocation::internal(),
	    integer_type());
	PointerType pointer_b(
	    SourceLocation::internal(),
	    integer_type());
	assert(&pointer_a != &pointer_b);
	assert(pointer_a.value_conversion_from(
	    &pointer_b));
	assert(pointer_a.same_cxx_carrier_as(
	    &pointer_b));

	Integer narrow_low(
	    1, integer_type());
	Integer narrow_high(
	    10, integer_type());
	Integer wide_low(
	    1, integer_type());
	Integer wide_high(
	    20, integer_type());
	Integer equal_low(
	    1, integer_type());
	Integer equal_high(
	    10, integer_type());
	SubrangeType narrow(
	    SourceLocation::internal(),
	    integer_type(), &narrow_low,
	    &narrow_high);
	SubrangeType wide(
	    SourceLocation::internal(),
	    integer_type(), &wide_low,
	    &wide_high);
	SubrangeType equal_narrow(
	    SourceLocation::internal(),
	    integer_type(), &equal_low,
	    &equal_high);
	assert(&narrow != &equal_narrow);
	assert(narrow.is_subtype_of(&wide));
	assert(!wide.is_subtype_of(&narrow));
	assert(narrow.is_subtype_of(
	    &equal_narrow));
	assert(equal_narrow.is_subtype_of(
	    &narrow));
	auto narrow_to_wide =
	    wide.value_conversion_from(&narrow);
	auto wide_to_narrow =
	    narrow.value_conversion_from(&wide);
	assert(narrow_to_wide);
	assert(!wide_to_narrow);
	assert(narrow_to_wide->kind ==
	       ValueConversionClass::Direct);
	assert(integer_type()->value_conversion_from(
	    &narrow));
	assert(!narrow.value_conversion_from(
	    integer_type()));

	FixedSetType narrow_set(
	    SourceLocation::internal(), &narrow);
	FixedSetType wide_set(
	    SourceLocation::internal(), &wide);
	assert(narrow_set.is_subtype_of(
	    &wide_set));
	assert(!wide_set.is_subtype_of(
	    &narrow_set));
	assert(wide_set.value_conversion_from(
	    &narrow_set));
	assert(!narrow_set.value_conversion_from(
	    &wide_set));

	Frame record_a_members(nullptr);
	Frame record_b_members(nullptr);
	RecordType record_a(
	    SourceLocation::internal(),
	    &record_a_members);
	RecordType record_b(
	    SourceLocation::internal(),
	    &record_b_members);
	assert(!record_a.is_subtype_of(
	    &record_b));
	assert(!record_a.value_conversion_from(
	    &record_b));
	assert(!record_a.same_cxx_carrier_as(
	    &record_b));

	assert(integer_type() != longint_type());
	assert(integer_type()
	           ->same_cxx_carrier_as(
	               longint_type()));

	Parameter value_parameter(
	    "value", "p_value", integer_type(),
	    ParamMode::Value, nullptr);
	Parameter var_parameter(
	    "value", "p_value", integer_type(),
	    ParamMode::Var, nullptr);
	RoutineType value_routine(
	    SourceLocation::internal(),
	    {value_parameter}, integer_type(),
	    ROUTINE);
	RoutineType same_value_routine(
	    SourceLocation::internal(),
	    {value_parameter}, integer_type(),
	    ROUTINE);
	RoutineType var_routine(
	    SourceLocation::internal(),
	    {var_parameter}, integer_type(),
	    ROUTINE);
	RoutineType method_routine(
	    SourceLocation::internal(),
	    {value_parameter}, integer_type(),
	    METHOD);
	assert(value_routine
	           .same_overload_signature_as(
	               &var_routine));
	assert(!value_routine.same_signature_as(
	    &var_routine));
	assert(value_routine.same_signature_as(
	    &same_value_routine));
	assert(value_routine
	           .same_overload_signature_as(
	               &method_routine));
	assert(!value_routine
	            .accepts_routine_value_from(
	                &method_routine));
}
