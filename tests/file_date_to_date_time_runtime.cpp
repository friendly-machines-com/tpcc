#include "rtl.h"

#include <cmath>
#include <cstdlib>
#include <ctime>

static bool nearly_equal(::u_system::t_double left, ::u_system::t_double right) {
	return std::abs(left - right) < 1.0e-10;
}

int main() {
	if (::setenv("TZ", "UTC0", 1) != 0) {
		return EXIT_FAILURE;
	}
	::tzset();
	if (!nearly_equal(::u_sysutils::p_filedatetodatetime(0), 25569.0)) {
		return EXIT_FAILURE;
	}
	if (!nearly_equal(::u_sysutils::p_filedatetodatetime(43200), 25569.5)) {
		return EXIT_FAILURE;
	}

	if (::setenv("TZ", "EST5", 1) != 0) {
		return EXIT_FAILURE;
	}
	::tzset();
	if (!nearly_equal(::u_sysutils::p_filedatetodatetime(0), 25568.0 + 19.0 / 24.0)) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
