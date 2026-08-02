#include "builtins.h"
#include "types.h"
#include <cassert>

int main() {
	// FPC's 64-bit System declarations are aliases, not equal-range nominal
	// types. Compiler-owned SizeOf/Length and external RTL-name lookup must
	// therefore return the canonical Int64/QWord Type* objects as well.
	assert(sizeint_type() == int64_type());
	assert(ptrint_type() == int64_type());
	assert(ptruint_type() == qword_type());
	assert(lookup_builtin_type(
		   "::u_system::t_sizeint") ==
	       int64_type());
	assert(lookup_builtin_type(
		   "::u_system::t_sizeuint") ==
	       qword_type());

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
	PointerType pointer_char(
	    SourceLocation::internal(),
	    char_type());
	assert(&pointer_a != &pointer_b);
	assert(pointer_a.value_conversion_from(
	    &pointer_b));
	assert(pointer_a.same_cxx_carrier_as(
	    &pointer_b));
	assert(pointer_char
	           .predefined_explicit_conversion_from(
		       &pointer_a));

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
	    "m_test_narrow",
	    integer_type(), &narrow_low,
	    &narrow_high);
	SubrangeType wide(
	    SourceLocation::internal(),
	    "m_test_wide",
	    integer_type(), &wide_low,
	    &wide_high);
	SubrangeType equal_narrow(
	    SourceLocation::internal(),
	    "m_test_equal_narrow",
	    integer_type(), &equal_low,
	    &equal_high);
	assert(&narrow != &equal_narrow);
	assert(!narrow.same_cxx_carrier_as(
	    &equal_narrow));
	assert(!narrow.same_cxx_carrier_as(
	    integer_type()));
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
	assert(record_a
	           .predefined_explicit_conversion_from(
		       &record_a));
	assert(!record_a
	            .predefined_explicit_conversion_from(
			&record_b));

	EnumType enumeration(
	    SourceLocation::internal(),
	    "m_test_enum", "zero", "one");
	assert(byte_type()
	           ->predefined_explicit_conversion_from(
		       &enumeration));
	assert(enumeration
	           .predefined_explicit_conversion_from(
		       integer_type()));
	assert(!integer_type()
	            ->predefined_explicit_conversion_from(
			double_type()));
	assert(narrow_set
	           .predefined_explicit_conversion_from(
		       &wide_set));

	Frame base_members(nullptr);
	Frame derived_members(nullptr);
	Frame unrelated_members(nullptr);
	ClassType base_class(
	    SourceLocation::internal(),
	    &base_members, {}, nullptr);
	ClassType derived_class(
	    SourceLocation::internal(),
	    &derived_members, {}, &base_class);
	ClassType unrelated_class(
	    SourceLocation::internal(),
	    &unrelated_members, {}, nullptr);
	ClassRefType derived_class_ref(
	    SourceLocation::internal(),
	    &derived_class);
	Frame interface_members(nullptr);
	InterfaceType interface_type(
	    SourceLocation::internal(),
	    &interface_members, {});
	Frame object_members(nullptr);
	ObjectType old_object(
	    SourceLocation::internal(),
	    &object_members, nullptr);
	assert(derived_class
	           .predefined_explicit_conversion_from(
		       &base_class));
	assert(base_class
	           .predefined_explicit_conversion_from(
		       &derived_class));
	assert(!unrelated_class
	            .predefined_explicit_conversion_from(
			&base_class));
	assert(pointer_a
	           .predefined_explicit_conversion_from(
		       &base_class));
	assert(base_class
	           .predefined_explicit_conversion_from(
		       &pointer_a));
	auto class_to_pointer =
	    pointer_type()->value_conversion_from(
		&derived_class);
	auto classref_to_pointer =
	    pointer_type()->value_conversion_from(
		&derived_class_ref);
	assert(class_to_pointer);
	assert(classref_to_pointer);
	assert(class_to_pointer->kind ==
	       ValueConversionClass::Convert);
	assert(classref_to_pointer->kind ==
	       ValueConversionClass::Convert);
	assert(class_to_pointer->distance >
	       base_class
		   .value_conversion_from(
		       &derived_class)
		   ->distance);
	assert(!pointer_a.value_conversion_from(
	    &derived_class));
	assert(!pointer_type()
	            ->value_conversion_from(
			&interface_type));
	assert(!pointer_type()
	            ->value_conversion_from(
			&old_object));

	Frame packed_members(nullptr);
	PackedRecordType packed_byte(
	    SourceLocation::internal(),
	    &packed_members);
	StorageSlot packed_byte_field(
	    "p_value", byte_type(),
	    StorageSlot::Kind::AggregateMember,
	    &packed_byte);
	packed_byte.fields.push_back(
	    AggregateField{
		"value", &packed_byte_field,
		byte_type()});
	assert(packed_byte
	           .predefined_explicit_conversion_from(
		       byte_type()));
	assert(byte_type()
	           ->predefined_explicit_conversion_from(
		       &packed_byte));
	assert(!packed_byte
	            .predefined_explicit_conversion_from(
			word_type()));

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
