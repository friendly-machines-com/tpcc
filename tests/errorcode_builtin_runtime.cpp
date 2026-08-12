#define main tpcc_pascal_main
#include "errorcode_builtin.cc"
#undef main

int main() {
	::u_system::p_errorcode = 0;
	if (tpcc_pascal_main() != 0)
		return 1;
	if (::u_system::p_errorcode != 23)
		return 2;
	return 0;
}
