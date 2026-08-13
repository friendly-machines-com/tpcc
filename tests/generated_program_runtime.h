#ifndef TPCC_TEST_GENERATED_PROGRAM_RUNTIME_H
#define TPCC_TEST_GENERATED_PROGRAM_RUNTIME_H

#ifndef TPCC_TEST_GENERATED_PROGRAM
#error "define TPCC_TEST_GENERATED_PROGRAM to the generated program's C++ file"
#endif

// Runtime tests need access to the program's globals after executing its
// Pascal body. Rename the emitted entry point while including that program so
// the test can supply its own C++ main.
#define main tpcc_test_pascal_main
#include TPCC_TEST_GENERATED_PROGRAM
#undef main
#undef TPCC_TEST_GENERATED_PROGRAM

inline int tpcc_run_generated_program()
{
	// Generated main installs argv before initializing units. Keep both the
	// argument text and vector alive until process exit because unit
	// finalization may legally call ParamStr.
	static char program_name[] = "tpcc-test";
	static char* arguments[] = {program_name, nullptr};
	return tpcc_test_pascal_main(1, arguments);
}

#endif
