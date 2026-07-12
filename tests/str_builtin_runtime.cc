#include "rtl.h"

#include <cstdlib>
#include <cstring>
#include <limits>

static bool equals(const pas::t_shortstring& value, const char* expected) {
	const std::size_t length = std::strlen(expected);
	return static_cast<std::size_t>(value.length) == length &&
	    std::memcmp(value.data, expected, length) == 0;
}

int main() {
	pas::t_shortstring text{};

	pas::p_str(static_cast<pas::t_extended>(1.5L), text);
	if (!equals(text, " 1.50000000000000000000E+0000"))
		return EXIT_FAILURE;

	pas::p_str(static_cast<pas::t_extended>(-0.125L), text);
	if (!equals(text, "-1.25000000000000000000E-0001"))
		return EXIT_FAILURE;

	pas::p_str(std::numeric_limits<pas::t_extended>::infinity(), text);
	if (!equals(text, "                         +Inf"))
		return EXIT_FAILURE;

	pas::p_str(std::numeric_limits<pas::t_extended>::quiet_NaN(), text);
	if (!equals(text, "                          Nan"))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
