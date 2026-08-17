#define TPCC_TEST_GENERATED_PROGRAM "new_designator.cc"
#include "generated_program_runtime.h"

int main() {
	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	if (p_finalplain != 11) {
		return 2;
	}
	if (!p_finalsetmember) {
		return 3;
	}
	return 0;
}
