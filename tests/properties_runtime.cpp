#define main tpcc_pascal_main
#include "properties.cc"
#undef main

int main() {
	if (tpcc_pascal_main() != 0)
		return 1;
	if (p_box.p_fdirect != 11)
		return 2;
	if (p_box.p_fvalue != 20)
		return 3;
	if (::u_system::p_index(p_box.p_fitems, 1) != 30 ||
	    ::u_system::p_index(p_box.p_fitems, 2) != 40)
		return 4;
	if (::u_system::p_index(p_child.p_fitems, 3) != 41)
		return 9;
	if (::u_system::p_index(p_grid.p_fcells, 1) != 42 ||
	    ::u_system::p_index(p_grid.p_fcells, 2) != 43)
		return 10;
	if (::u_system::p_index(p_a, 1) != 51)
		return 5;
	if (p_p != std::addressof(p_box.p_fdirect))
		return 6;
	if (p_c != static_cast<::u_system::t_char>('a') ||
	    ::u_system::p_index(p_s, 2) != static_cast<::u_system::t_char>('a'))
		return 7;
	if (p_ac != static_cast<::u_system::t_char>('a') ||
	    ::u_system::p_index(p_longs, 1) != static_cast<::u_system::t_char>('a'))
		return 12;
	if (p_cp != std::addressof(::u_system::p_index(p_longs, 1)))
		return 13;
	if (p_seenbyte != 1 || p_seenchar != 1)
		return 11;
	if (p_x != 278)
		return 8;
	return 0;
}
