#define main tpcc_pascal_main
#include "include_exclude_builtin.cc"
#undef main

int main() {
	if (tpcc_pascal_main() != 0)
		return 1;
	if (p_tenpresent != pas::p_true ||
	    p_elevenpresent != pas::p_false ||
	    p_twelvepresent != pas::p_false ||
	    p_thirteenpresent != pas::p_true ||
	    p_fifteenpresent != pas::p_false)
		return 2;
	if (p_nulpresent != pas::p_false ||
	    p_apresent != pas::p_true)
		return 3;
	return 0;
}
