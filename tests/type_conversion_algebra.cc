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

	assert(shortint_to_int64);
	assert(shortint_to_single);
	assert(byte_to_int64);
	assert(integer_to_cardinal);
	assert(integer_to_int64);
	assert(shortint_to_int64->kind ==
	       ValueConversionClass::Convert);
	assert(shortint_to_single->kind ==
	       ValueConversionClass::Convert);
	assert(shortint_to_int64->distance <
	       shortint_to_single->distance);
	assert(integer_to_int64->distance <
	       integer_to_cardinal->distance);
}
