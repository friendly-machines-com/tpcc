#define TPCC_TEST_GENERATED_PROGRAM "writable_cast.cc"
#include "generated_program_runtime.h"

int main() {
	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	if (p_c.value != 255) {
		return 2;
	}
	if (p_signed != -1) {
		return 3;
	}
	if (p_unsigned != 255) {
		return 4;
	}
	if (p_values.items[0] != -1) {
		return 5;
	}
	if (p_indexcalls != 1) {
		return 6;
	}
	if (p_changedfirst == p_originalfirst) {
		return 7;
	}
	if (p_changedbyvar == p_realbytes.items[1]) {
		return 8;
	}
	if (p_realvalue != static_cast<::u_system::t_double>(1.5)) {
		return 9;
	}
	return 0;
}
