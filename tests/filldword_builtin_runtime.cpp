#include "rtl.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>

struct runtime_error_code {
	::u_system::t_longint value;
};

static void raise_runtime_error(::u_system::t_longint value, ::u_system::t_pointer, ::u_system::t_pointer) {
	throw runtime_error_code{value};
}

int main() {
	::u_system::p_errorproc = &raise_runtime_error;

	std::array<std::byte, 6> filled_bytes{};
	filled_bytes.front() = std::byte{0x5a};
	filled_bytes.back() = std::byte{0xa5};
	::u_system::p_fillbyte({filled_bytes.data() + 1, 4}, 4, 0x3c);
	for (std::size_t i = 1; i < 5; ++i) {
		if (filled_bytes[i] != std::byte{0x3c}) {
			return EXIT_FAILURE;
		}
	}
	if (filled_bytes.front() != std::byte{0x5a} || filled_bytes.back() != std::byte{0xa5}) {
		return EXIT_FAILURE;
	}
	::u_system::p_fillbyte({filled_bytes.data() + 1, 4}, 0, 0);
	::u_system::p_fillbyte({filled_bytes.data() + 1, 4}, -1, 0);

	std::array<std::byte, 10> bytes{};
	bytes.front() = std::byte{0x5a};
	bytes.back() = std::byte{0xa5};
	const ::u_system::t_longword value = 0x12345678u;
	::u_system::p_filldword({bytes.data() + 1, 8}, 2, value);

	::u_system::t_longword first = 0;
	::u_system::t_longword second = 0;
	std::memcpy(&first, bytes.data() + 1, sizeof(first));
	std::memcpy(&second, bytes.data() + 5, sizeof(second));
	if (bytes.front() != std::byte{0x5a} || bytes.back() != std::byte{0xa5} || first != value || second != value) {
		return EXIT_FAILURE;
	}

	::u_system::p_filldword({bytes.data() + 1, 8}, 0, 0);
	::u_system::p_filldword({bytes.data() + 1, 8}, -1, 0);
	std::memcpy(&first, bytes.data() + 1, sizeof(first));
	if (first != value) {
		return EXIT_FAILURE;
	}

	bool rejected = false;
	try {
		::u_system::p_filldword({bytes.data() + 1, 8}, 3, value);
	} catch (const runtime_error_code& error) {
		rejected = error.value == 201;
	}
	if (!rejected) {
		return EXIT_FAILURE;
	}

	rejected = false;
	try {
		::u_system::p_filldword({bytes.data() + 1, std::numeric_limits<std::size_t>::max()}, std::numeric_limits<::u_system::t_sizeint>::max(), value);
	} catch (const runtime_error_code& error) {
		rejected = error.value == 201;
	}
	if (!rejected) {
		return EXIT_FAILURE;
	}

	rejected = false;
	try {
		::u_system::p_fillbyte({filled_bytes.data() + 1, 4}, 5, 0);
	} catch (const runtime_error_code& error) {
		rejected = error.value == 201;
	}
	if (!rejected) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
