#include "rtl.h"

#include <cstdlib>
#include <cstring>

static bool equals(const ::u_system::t_shortstring<255>& value, const char* expected) {
	const std::size_t length = std::strlen(expected);
	return static_cast<std::size_t>(value.length) == length && std::memcmp(value.data, expected, length) == 0;
}

int main() {
	if (!equals(::u_system::p_octstr(static_cast<::u_system::t_longint>(83), 3), "123")) {
		return EXIT_FAILURE;
	}
	if (!equals(::u_system::p_octstr(static_cast<::u_system::t_longint>(83), 5), "00123")) {
		return EXIT_FAILURE;
	}
	if (!equals(::u_system::p_octstr(static_cast<::u_system::t_longint>(83), 2), "23")) {
		return EXIT_FAILURE;
	}
	if (!equals(::u_system::p_octstr(static_cast<::u_system::t_longint>(-1), 12), "777777777777")) {
		return EXIT_FAILURE;
	}
	if (!equals(::u_system::p_octstr(static_cast<::u_system::t_int64>(8), 3), "010")) {
		return EXIT_FAILURE;
	}
	if (!equals(::u_system::p_octstr(static_cast<::u_system::t_qword>(-1), 22), "7777777777777777777777")) {
		return EXIT_FAILURE;
	}
	if (!equals(::u_system::p_octstr(static_cast<::u_system::t_qword>(0), 0), "")) {
		return EXIT_FAILURE;
	}

	auto maximum = ::u_system::p_octstr(static_cast<::u_system::t_longint>(0), 255);
	if (maximum.length != 255) {
		return EXIT_FAILURE;
	}
	for (std::size_t i = 0; i < 255; ++i) {
		if (maximum.data[i] != '0') {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
