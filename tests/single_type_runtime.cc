#define main tpcc_pascal_main
#include "single_type.cc"
#undef main

int main() {
	static_assert(sizeof(pas::t_single) == 4);
	if (tpcc_pascal_main() != 0)
		return 1;
	if (p_foldedcast != 1.25f ||
	    p_foldedcoerce != 2.5f)
		return 2;
	if (p_s != 3.75f ||
	    p_d != 3.75 ||
	    p_e != 3.75L)
		return 3;
	if (p_casted != 3.75f ||
	    p_coerced != 3.75f)
		return 4;
	if (p_sum != 4.0f ||
	    p_product != 7.5f ||
	    p_quotient != 3.75f)
		return 5;
	if (p_singleless != pas::p_true ||
	    p_singlesize != 4)
		return 6;
	if (p_ranksingle != 1 ||
	    p_rankdouble != 2 ||
	    p_rankextended != 3 ||
	    p_rankinteger != 1 ||
	    p_rankliteral != 3)
		return 7;
	if (p_parsecode != 0 ||
	    p_parsed != 2.25f)
		return 8;
	return 0;
}
