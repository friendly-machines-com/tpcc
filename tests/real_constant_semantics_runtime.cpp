#include <cmath>
#include <limits>

#define main tpcc_pascal_main
#include "real_constant_semantics.cc"
#undef main

int main() {
	if (tpcc_pascal_main() != 0) {
		return 1;
	}
	if (p_exactrank != 1 || p_exactaliasrank != p_exactrank) {
		return 2;
	}
	if (p_roundedrank != 3 || p_roundedaliasrank != p_roundedrank) {
		return 3;
	}
	if (p_foldedrank != 3) {
		return 4;
	}
	if (p_exactmixedrank != 1 || p_roundedmixedrank != 3) {
		return 5;
	}
	if (p_dividerank != 1) {
		return 6;
	}
	if (p_foldedz != p_runtimez) {
		return 7;
	}
	if (p_foldedsingleboundary != 16777216.0f) {
		return 8;
	}
	if (p_foldeddoubleboundary != 9007199254740992.0) {
		return 9;
	}
	if (!__builtin_signbit(p_foldednegativezero)) {
		return 10;
	}
	if (p_foldedpositiveexponent != 125.0f) {
		return 11;
	}
	if (p_foldedminimuminteger != std::numeric_limits<::u_system::t_int64>::min() || p_foldedmaximuminteger != std::numeric_limits<::u_system::t_qword>::max()) {
		return 12;
	}
	if (p_exactoriginsize != sizeof(::u_system::t_single) || p_roundedoriginsize != sizeof(::u_system::t_extended)) {
		return 13;
	}
	if (p_roundedintegerrank != 2 || p_wideintegerrank != 3 || p_wideintegermixedrank != 3) {
		return 14;
	}
	if (p_tinysingle != 0.0f || !__builtin_isfinite(p_tinysingle) || p_tinydouble != 0.0 || !__builtin_isfinite(p_tinydouble) || p_tinyextended != 0.0L || !__builtin_isfinite(p_tinyextended)) {
		return 15;
	}
	return 0;
}
