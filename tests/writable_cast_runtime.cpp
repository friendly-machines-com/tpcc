#define main tpcc_pascal_main
#include "writable_cast.cc"
#undef main

int main() {
	if (tpcc_pascal_main() != 0) {
		return 1;
	}
	if (p_c.value != 255) {
		return 2;
	}
	if (p_signed != -1) {
		return 3;
	}
	if (p_unsigned != 255) {
		return 4;
	}
	if (p_values.items[0] != -1) {
		return 5;
	}
	if (p_indexcalls != 1) {
		return 6;
	}
	return 0;
}
