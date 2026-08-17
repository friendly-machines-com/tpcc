#define TPCC_TEST_GENERATED_PROGRAM "writable_class_cast.cc"
#include "generated_program_runtime.h"

int main() {
	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	// Replace() stored a fresh TTempCreateNode through the widened var view.
	if (p_finalv != 11) {
		return 2;
	}
	// The replacement instance was default-constructed before p.v := 11, so
	// the derived field proves the slot now holds a different object.
	if (p_finalw != 0) {
		return 3;
	}
	return 0;
}
