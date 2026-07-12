#include "rtl.h"

int main() {
	auto text = pas::tpcc_shortstring_from_c("abc");
	if (pas::p_pos('b', text) != 2)
		return 1;
	if (pas::p_pos('x', text) != 0)
		return 2;
	return 0;
}
