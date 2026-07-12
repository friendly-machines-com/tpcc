#include <sstream>

#define main tpcc_pascal_main
#include "write_builtin.cc"
#undef main

int main() {
	std::ostringstream standard_output;
	std::ostringstream file_output;
	std::streambuf* old_output =
	    std::cout.rdbuf(standard_output.rdbuf());
	p_destination.stream = &file_output;

	int result = tpcc_pascal_main();
	std::cout.rdbuf(old_output);

	if (result != 0)
		return 1;
	if (standard_output.str() !=
	    "A12 Z!\n\nTRUE\n  12\n")
		return 2;
	if (file_output.str() != "file=-7:2.50\n")
		return 3;
	return 0;
}
