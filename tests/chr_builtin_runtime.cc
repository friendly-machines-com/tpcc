#define main tpcc_pascal_main
#include "chr_builtin.cc"
#undef main

int main() {
	if (tpcc_pascal_main() != 0)
		return 1;
	if (p_foldedvalue != pas::t_char{65})
		return 2;
	if (p_runtimevalue != pas::t_char{255})
		return 3;
	return 0;
}
