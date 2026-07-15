#include "rtl.h"

#include <limits>
#include <stdexcept>

int main() {
	try {
		(void)::u_system::p_trunc(std::numeric_limits<::u_system::t_extended>::infinity());
		return 1;
	} catch (const std::range_error&) {
	}

	try {
		(void)::u_system::p_round(std::numeric_limits<::u_system::t_extended>::quiet_NaN());
		return 2;
	} catch (const std::range_error&) {
	}

	return 0;
}
