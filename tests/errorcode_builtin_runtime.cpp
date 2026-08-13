#define TPCC_TEST_GENERATED_PROGRAM "errorcode_builtin.cc"
#include "generated_program_runtime.h"

int main() {
	::u_system::p_errorcode = 0;
	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	if (::u_system::p_errorcode != 23) {
		return 2;
	}
	return 0;
}
