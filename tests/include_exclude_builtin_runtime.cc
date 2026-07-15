#define main tpcc_pascal_main
#include "include_exclude_builtin.cc"
#undef main

int main() {
	if (tpcc_pascal_main() != 0)
		return 1;
	if (p_tenpresent != ::u_system::p_true ||
	    p_elevenpresent != ::u_system::p_false ||
	    p_twelvepresent != ::u_system::p_false ||
	    p_thirteenpresent != ::u_system::p_true ||
	    p_fifteenpresent != ::u_system::p_false)
		return 2;
	if (p_nulpresent != ::u_system::p_false ||
	    p_apresent != ::u_system::p_true)
		return 3;
	return 0;
}
