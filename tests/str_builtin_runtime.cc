#include "rtl.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

static bool equals(
    const ::u_system::t_shortstring<255>& value,
    const char* expected) {
	const std::size_t length = std::strlen(expected);
	return static_cast<std::size_t>(value.length) == length &&
	    std::memcmp(value.data, expected, length) == 0;
}

static bool equals(
    const ::u_system::t_ansistring& value,
    const char* expected) {
	return value.m_string() == expected;
}

template<typename T>
static std::string render(
    const ::u_system::tpcc_formatted_value<T>& value) {
	const auto rendered =
	    ::u_system::tpcc_render_formatted_value(
		value);
	return std::string(
		   rendered.left_padding, ' ') +
	    rendered.value;
}

int main() {
	::u_system::t_shortstring<255> text{};
	::u_system::t_ansistring dynamic_text{};

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
	::u_system::p_str(integer, text);
	if (!equals(
		text,
		render(integer).c_str()))
		return EXIT_FAILURE;

	auto extended =
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_extended>(
		    1.5L));
	::u_system::p_str(extended, text);
	if (!equals(
		text,
		render(extended).c_str()))
		return EXIT_FAILURE;

	auto precise =
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_extended>(
		    1.5L),
		static_cast<::u_system::t_sizeint>(0),
		static_cast<::u_system::t_sizeint>(3));
	::u_system::p_str(precise, dynamic_text);
	if (!equals(dynamic_text, "1.500"))
		return EXIT_FAILURE;

	auto wide =
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_integer>(7),
		static_cast<::u_system::t_sizeint>(300));
	::u_system::p_str(wide, dynamic_text);
	if (dynamic_text.m_length() != 300 ||
	    dynamic_text.index(1).value != ' ' ||
	    dynamic_text.index(299).value != ' ' ||
	    dynamic_text.index(300).value != '7')
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
