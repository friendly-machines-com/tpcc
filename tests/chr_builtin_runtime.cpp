#define TPCC_TEST_GENERATED_PROGRAM "chr_builtin.cc"
#include "generated_program_runtime.h"

int main() {
	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	if (p_foldedvalue != ::u_system::t_char{65}) {
		return 2;
	}
	if (p_runtimevalue != ::u_system::t_char{255}) {
		return 3;
	}
	return 0;
}
