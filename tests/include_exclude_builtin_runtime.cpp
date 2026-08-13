#define TPCC_TEST_GENERATED_PROGRAM "include_exclude_builtin.cc"
#include "generated_program_runtime.h"

int main() {
	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	if (p_tenpresent != ::u_system::p_true || p_elevenpresent != ::u_system::p_false || p_twelvepresent != ::u_system::p_false || p_thirteenpresent != ::u_system::p_true || p_fifteenpresent != ::u_system::p_false) {
		return 2;
	}
	if (p_nulpresent != ::u_system::p_false || p_apresent != ::u_system::p_true) {
		return 3;
	}
	return 0;
}
