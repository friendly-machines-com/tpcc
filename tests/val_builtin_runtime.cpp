#include "rtl.h"

#include <cmath>
#include <cstdlib>
#include <limits>

template <typename T> static bool parse(const char* text, std::size_t length, T expected) {
	::u_system::t_shortstring<255> source = ::u_system::tpcc_shortstring_from_c(text, length);
	T value{};
	::u_system::t_integer code = -1;
	::u_system::p_val(source, value, code);
	return code == 0 && value == expected;
}

int main() {
	if (!parse("-128", 4, std::numeric_limits<::u_system::t_shortint>::min())) {
		return EXIT_FAILURE;
	}
	if (!parse("$FF", 3, static_cast<::u_system::t_shortint>(-1))) {
		return EXIT_FAILURE;
	}
	if (!parse("  -1600", 7, static_cast<::u_system::t_smallint>(-1600))) {
		return EXIT_FAILURE;
	}
	if (!parse("320000", 6, static_cast<::u_system::t_longint>(320000))) {
		return EXIT_FAILURE;
	}
	if (!parse("-640000", 7, static_cast<::u_system::t_int64>(-640000))) {
		return EXIT_FAILURE;
	}
	if (!parse("%11111111", 9, std::numeric_limits<::u_system::t_byte>::max())) {
		return EXIT_FAILURE;
	}
	if (!parse("&177777", 7, std::numeric_limits<::u_system::t_word>::max())) {
		return EXIT_FAILURE;
	}
	if (!parse("0xFFFFFFFF", 10, std::numeric_limits<::u_system::t_longword>::max())) {
		return EXIT_FAILURE;
	}
	if (!parse("$FFFFFFFFFFFFFFFF", 17, std::numeric_limits<::u_system::t_qword>::max())) {
		return EXIT_FAILURE;
	}
	if (!parse("1.25", 4, static_cast<::u_system::t_double>(1.25))) {
		return EXIT_FAILURE;
	}
	if (!parse("-2.5e2", 6, static_cast<::u_system::t_extended>(-250.0L))) {
		return EXIT_FAILURE;
	}

	::u_system::t_shortstring<255> source = ::u_system::tpcc_shortstring_from_c("128", 3);
	::u_system::t_shortint small = 42;
	::u_system::t_integer code = -1;
	::u_system::p_val(source, small, code);
	if (small != 0 || code != 3) {
		return EXIT_FAILURE;
	}

	source = ::u_system::tpcc_shortstring_from_c("12x", 3);
	::u_system::t_longint integer = 42;
	::u_system::p_val(source, integer, code);
	if (integer != 0 || code != 3) {
		return EXIT_FAILURE;
	}
	::u_system::t_byte byte_code = 0;
	::u_system::p_val(source, integer, ::u_system::tpcc_make_storage_ref(byte_code));
	if (integer != 0 || byte_code != 3) {
		return EXIT_FAILURE;
	}

	source = ::u_system::tpcc_shortstring_from_c("", 0);
	::u_system::t_double real = 42;
	::u_system::p_val(source, real, code);
	if (real != 0 || code != 1) {
		return EXIT_FAILURE;
	}

	source = ::u_system::tpcc_shortstring_from_c("17", 2);
	::u_system::p_val(source, integer);
	if (integer != 17) {
		return EXIT_FAILURE;
	}

	std::string long_invalid(300, ' ');
	long_invalid.push_back('z');
	::u_system::t_ansistring ansi_source = ::u_system::tpcc_ansistring_literal(long_invalid.data(), long_invalid.size());
	::u_system::t_word word_code = 0;
	::u_system::p_val(ansi_source, integer, ::u_system::tpcc_make_storage_ref(word_code));
	if (integer != 0 || word_code != 301) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
