#include "rtl.h"

#include <cstdlib>

int main() {
	::u_system::t_pointer zeroed =
	    ::u_system::p_allocmem(16);
	if (!zeroed)
		return EXIT_FAILURE;
	auto* bytes =
	    static_cast<unsigned char*>(zeroed);
	for (std::size_t i = 0; i < 16; ++i)
		if (bytes[i] != 0)
			return EXIT_FAILURE;
	if (::u_system::p_freemem(zeroed) != 0)
		return EXIT_FAILURE;

	::u_system::t_char* characters = nullptr;
	::u_system::p_getmem(characters, 8);
	if (!characters)
		return EXIT_FAILURE;
	characters[0] = ::u_system::t_char{'a'};
	characters[1] = ::u_system::t_char{0};
	::u_system::t_char* returned =
	    ::u_system::p_reallocmem(characters, 16);
	if (returned != characters)
		return EXIT_FAILURE;
	if (characters[0] != ::u_system::t_char{'a'})
		return EXIT_FAILURE;
	returned = ::u_system::p_reallocmem(characters, 32);
	if (returned != characters ||
	    characters[0] != ::u_system::t_char{'a'})
		return EXIT_FAILURE;
	returned = ::u_system::p_reallocmem(characters, 4);
	if (returned != characters ||
	    characters[0] != ::u_system::t_char{'a'})
		return EXIT_FAILURE;
	if (::u_system::p_reallocmem(characters, 0) != nullptr ||
	    characters != nullptr)
		return EXIT_FAILURE;

	::u_system::p_getmem(characters, 8);
	if (::u_system::p_freemem(characters) != 0)
		return EXIT_FAILURE;

	::u_system::t_pointer raw = nullptr;
	::u_system::t_pointer raw_returned =
	    ::u_system::p_reallocmem(raw, 8);
	if (raw_returned != raw || !raw)
		return EXIT_FAILURE;
	if (::u_system::p_reallocmem(raw, 0) != nullptr || raw)
		return EXIT_FAILURE;

	raw = ::u_system::p_getmem(8);
	if (!raw)
		return EXIT_FAILURE;
	::u_system::p_freemem(raw, 8);

	raw = ::u_system::p_allocmem(0);
	::u_system::p_freemem(raw);

	return EXIT_SUCCESS;
}
