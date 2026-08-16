#include <bit>
#include <cstdint>

#define TPCC_TEST_GENERATED_PROGRAM "currency_range_modes.cc"
#include "generated_program_runtime.h"

int main() {
	constexpr std::uint64_t expected_bits = std::uint64_t{922337203685478} * std::uint64_t{10000};
	constexpr std::int64_t expected = std::bit_cast<std::int64_t>(expected_bits);

	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	if (::u_system::m_currency_raw(p_wrappedinteger) != expected || ::u_system::m_currency_raw(p_wrappedtypedinteger) != expected || ::u_system::m_currency_raw(p_runtimewrapped) != expected) {
		return 2;
	}
	if (::u_system::m_currency_raw(p_wrappedreal) != INT64_MIN) {
		return 3;
	}
	return 0;
}
