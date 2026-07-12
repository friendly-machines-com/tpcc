#include "rtl.h"

#include <limits>
#include <stdexcept>

int main() {
	try {
		(void)pas::p_trunc(std::numeric_limits<pas::t_extended>::infinity());
		return 1;
	} catch (const std::range_error&) {
	}

	try {
		(void)pas::p_round(std::numeric_limits<pas::t_extended>::quiet_NaN());
		return 2;
	} catch (const std::range_error&) {
	}

	return 0;
}
