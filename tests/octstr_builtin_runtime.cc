#include "rtl.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

static bool equals(const pas::t_shortstring& value, const char* expected) {
	const std::size_t length = std::strlen(expected);
	return static_cast<std::size_t>(value.length) == length &&
	    std::memcmp(value.data, expected, length) == 0;
}

int main() {
	if (!equals(pas::p_octstr(static_cast<pas::t_longint>(83), 3), "123"))
		return EXIT_FAILURE;
	if (!equals(pas::p_octstr(static_cast<pas::t_longint>(83), 5), "00123"))
		return EXIT_FAILURE;
	if (!equals(pas::p_octstr(static_cast<pas::t_longint>(83), 2), "23"))
		return EXIT_FAILURE;
	if (!equals(pas::p_octstr(static_cast<pas::t_longint>(-1), 12),
	            "777777777777"))
		return EXIT_FAILURE;
	if (!equals(pas::p_octstr(static_cast<pas::t_int64>(8), 3), "010"))
		return EXIT_FAILURE;
	if (!equals(pas::p_octstr(static_cast<pas::t_qword>(-1), 22),
	            "7777777777777777777777"))
		return EXIT_FAILURE;
	if (!equals(pas::p_octstr(static_cast<pas::t_qword>(0), 0), ""))
		return EXIT_FAILURE;

	try {
		(void)pas::p_octstr(static_cast<pas::t_longint>(0), 255);
		return EXIT_FAILURE;
	} catch (const std::length_error&) {
	}

	return EXIT_SUCCESS;
}
