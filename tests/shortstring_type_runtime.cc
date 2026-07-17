#include "rtl.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

int main() {
	static_assert(sizeof(::u_system::t_shortstring<1>) == 2);
	static_assert(sizeof(::u_system::t_shortstring<22>) == 23);
	static_assert(sizeof(::u_system::t_shortstring<255>) == 256);
	static_assert(alignof(::u_system::t_shortstring<22>) == 1);
	static_assert(
	    std::is_trivially_copyable_v<::u_system::t_shortstring<22>>);
	static_assert(std::is_aggregate_v<::u_system::t_shortstring<22>>);
	static_assert(
	    std::is_trivially_default_constructible_v<
	        ::u_system::t_shortstring<22>>);
	static_assert(
	    sizeof(::u_system::t_ansistring) == sizeof(void*));
	static_assert(
	    alignof(::u_system::t_ansistring) == alignof(void*));
	static_assert(
	    !std::is_trivially_copyable_v<
	        ::u_system::t_ansistring>);

	std::array<char, 255> payload{};
	for (std::size_t i = 0; i < payload.size(); ++i)
		payload[i] = static_cast<char>(i);
	auto full = ::u_system::tpcc_shortstring_from_c<255>(
	    payload.data(), payload.size());
	if (full.length.value != 255 ||
	    full.data[254].value !=
	        static_cast<unsigned char>(payload[254]))
		return EXIT_FAILURE;

	::u_system::t_shortstring<2> tiny =
	    ::u_system::tpcc_shortstring_cast<2>(full);
	if (tiny.length.value != 2 ||
	    tiny.data[0].value != 0 ||
	    tiny.data[1].value != 1)
		return EXIT_FAILURE;

	::u_system::t_shortstring<5> wider =
	    ::u_system::tpcc_shortstring_cast<5>(tiny);
	if (wider.length.value != 2 ||
	    wider.data[1].value != 1)
		return EXIT_FAILURE;

	wider.data[2] = ::u_system::t_char{'c'};
	wider.data[3] = ::u_system::t_char{'d'};
	::u_system::p_setlength(
	    ::u_system::tpcc_make_storage_ref(wider),
	    4);
	if (wider.length.value != 4 ||
	    wider.data[2].value != 'c' ||
	    wider.data[3].value != 'd')
		return EXIT_FAILURE;
	::u_system::p_setlength(
	    ::u_system::tpcc_make_storage_ref(wider),
	    99);
	if (wider.length.value != 5)
		return EXIT_FAILURE;
	::u_system::p_setlength(
	    ::u_system::tpcc_make_storage_ref(wider),
	    -1);
	if (wider.length.value != 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
