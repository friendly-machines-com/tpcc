#define main tpcc_pascal_main
#include "set_literals.cc"
#undef main

int main() {
	if (tpcc_pascal_main() != 0)
		return 1;
	if (p_integerhit != pas::p_true ||
	    p_integermiss != pas::p_false ||
	    p_rangehit != pas::p_true ||
	    p_emptymiss != pas::p_false)
		return 2;
	if (p_tabhit != pas::p_true ||
	    p_letterhit != pas::p_true ||
	    p_charactermiss != pas::p_false)
		return 3;
	if (p_enumhit != pas::p_true ||
	    p_enummiss != pas::p_false)
		return 4;
	return 0;
}
