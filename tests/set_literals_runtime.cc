#define main tpcc_pascal_main
#include "set_literals.cc"
#undef main

int main() {
	if (tpcc_pascal_main() != 0)
		return 1;
	if (p_integerhit != ::u_system::p_true ||
	    p_integermiss != ::u_system::p_false ||
	    p_rangehit != ::u_system::p_true ||
	    p_emptymiss != ::u_system::p_false)
		return 2;
	if (p_tabhit != ::u_system::p_true ||
	    p_letterhit != ::u_system::p_true ||
	    p_charactermiss != ::u_system::p_false)
		return 3;
	if (p_enumhit != ::u_system::p_true ||
	    p_enummiss != ::u_system::p_false)
		return 4;
	if (p_directhit != ::u_system::p_true ||
	    p_directemptymiss !=
		::u_system::p_false)
		return 5;
	return 0;
}
