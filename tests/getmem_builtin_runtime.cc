#include "rtl.h"

#include <cstdlib>

int main() {
	::u_system::t_char* characters = nullptr;
	::u_system::p_getmem(characters, 8);
	if (!characters)
		return EXIT_FAILURE;
	characters[0] = ::u_system::t_char{'a'};
	characters[1] = ::u_system::t_char{0};
	if (::u_system::p_freemem(characters) != 0)
		return EXIT_FAILURE;

	::u_system::t_pointer raw = ::u_system::p_getmem(8);
	if (!raw)
		return EXIT_FAILURE;
	::u_system::p_freemem(raw, 8);

	return EXIT_SUCCESS;
}
