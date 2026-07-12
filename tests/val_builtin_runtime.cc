#include "rtl.h"

#include <cmath>
#include <cstdlib>
#include <limits>

template<typename T>
static bool parse(const char* text, std::size_t length, T expected) {
	pas::t_shortstring source = pas::tpcc_shortstring_from_c(text, length);
	T value{};
	pas::t_integer code = -1;
	pas::p_val(source, value, code);
	return code == 0 && value == expected;
}

int main() {
	if (!parse("-128", 4, std::numeric_limits<pas::t_shortint>::min()))
		return EXIT_FAILURE;
	if (!parse("$FF", 3, static_cast<pas::t_shortint>(-1)))
		return EXIT_FAILURE;
	if (!parse("  -1600", 7, static_cast<pas::t_smallint>(-1600)))
		return EXIT_FAILURE;
	if (!parse("320000", 6, static_cast<pas::t_longint>(320000)))
		return EXIT_FAILURE;
	if (!parse("-640000", 7, static_cast<pas::t_int64>(-640000)))
		return EXIT_FAILURE;
	if (!parse("%11111111", 9, std::numeric_limits<pas::t_byte>::max()))
		return EXIT_FAILURE;
	if (!parse("&177777", 7, std::numeric_limits<pas::t_word>::max()))
		return EXIT_FAILURE;
	if (!parse("0xFFFFFFFF", 10, std::numeric_limits<pas::t_longword>::max()))
		return EXIT_FAILURE;
	if (!parse("$FFFFFFFFFFFFFFFF", 17, std::numeric_limits<pas::t_qword>::max()))
		return EXIT_FAILURE;
	if (!parse("1.25", 4, static_cast<pas::t_double>(1.25)))
		return EXIT_FAILURE;
	if (!parse("-2.5e2", 6, static_cast<pas::t_extended>(-250.0L)))
		return EXIT_FAILURE;

	pas::t_shortstring source = pas::tpcc_shortstring_from_c("128", 3);
	pas::t_shortint small = 42;
	pas::t_integer code = -1;
	pas::p_val(source, small, code);
	if (small != 0 || code != 3)
		return EXIT_FAILURE;

	source = pas::tpcc_shortstring_from_c("12x", 3);
	pas::t_longint integer = 42;
	pas::p_val(source, integer, code);
	if (integer != 0 || code != 3)
		return EXIT_FAILURE;
	pas::t_byte byte_code = 0;
	pas::p_val(
	    source,
	    integer,
	    pas::tpcc_make_storage_ref(byte_code));
	if (integer != 0 || byte_code != 3)
		return EXIT_FAILURE;

	source = pas::tpcc_shortstring_from_c("", 0);
	pas::t_double real = 42;
	pas::p_val(source, real, code);
	if (real != 0 || code != 1)
		return EXIT_FAILURE;

	source = pas::tpcc_shortstring_from_c("17", 2);
	pas::p_val(source, integer);
	if (integer != 17)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
