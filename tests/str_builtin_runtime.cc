#include "rtl.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>

static bool equals(
    const ::u_system::t_shortstring<255>& value,
    const char* expected) {
	const std::size_t length = std::strlen(expected);
	return static_cast<std::size_t>(value.length) == length &&
	    std::memcmp(value.data, expected, length) == 0;
}

int main() {
	::u_system::t_shortstring<255> text{};

	::u_system::p_str(static_cast<::u_system::t_extended>(1.5L), text);
	if (!equals(text, " 1.50000000000000000000E+0000"))
		return EXIT_FAILURE;

	::u_system::p_str(static_cast<::u_system::t_extended>(-0.125L), text);
	if (!equals(text, "-1.25000000000000000000E-0001"))
		return EXIT_FAILURE;

	::u_system::p_str(std::numeric_limits<::u_system::t_extended>::infinity(), text);
	if (!equals(text, "                         +Inf"))
		return EXIT_FAILURE;

	::u_system::p_str(std::numeric_limits<::u_system::t_extended>::quiet_NaN(), text);
	if (!equals(text, "                          Nan"))
		return EXIT_FAILURE;

	// Write and Str consume the same formatted-value object and therefore
	// cannot drift in numeric rendering or field-width behavior. Only their
	// sinks differ.
	auto integer =
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_integer>(42),
		static_cast<::u_system::t_sizeint>(5));
	std::ostringstream integer_output;
	if (::u_system::m_do_write(
		integer_output, integer) != 0)
		return EXIT_FAILURE;
	::u_system::p_str(integer, text);
	if (!equals(
		text,
		integer_output.str().c_str()))
		return EXIT_FAILURE;

	auto extended =
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_extended>(
		    1.5L));
	std::ostringstream extended_output;
	if (::u_system::m_do_write(
		extended_output, extended) != 0)
		return EXIT_FAILURE;
	::u_system::p_str(extended, text);
	if (!equals(
		text,
		extended_output.str().c_str()))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
