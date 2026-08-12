#include "rtl.h"

#include <cstdlib>

int main() {
	if (::u_system::p_strlen(nullptr) != 0) {
		return EXIT_FAILURE;
	}

	const ::u_system::t_char text[] = {
	    ::u_system::t_char{'a'}, ::u_system::t_char{'b'}, ::u_system::t_char{0}, ::u_system::t_char{'c'}, ::u_system::t_char{0},
	};
	if (::u_system::p_strlen(text) != 2) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_strlen(text + 3) != 1) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
