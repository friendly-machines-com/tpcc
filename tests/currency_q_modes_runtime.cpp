#include <cstdint>

#ifndef TPCC_TEST_GENERATED_PROGRAM
#define TPCC_TEST_GENERATED_PROGRAM "currency_q_modes.cc"
#endif
#include "generated_program_runtime.h"

int main() {
	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	if (::u_system::m_currency_raw(p_foldedwrapped) != INT64_MIN || ::u_system::m_currency_raw(p_runtimewrapped) != INT64_MIN) {
		return 2;
	}
	return 0;
}
