#include "rtl.h"

#include <limits>

struct runtime_error_code {
	::u_system::t_longint value;
};

static void raise_runtime_error(::u_system::t_longint value, ::u_system::t_pointer, ::u_system::t_pointer) {
	throw runtime_error_code{value};
}

int main() {
	::u_system::p_errorproc = &raise_runtime_error;
	try {
		(void)::u_system::p_trunc(std::numeric_limits<::u_system::t_extended>::infinity());
		return 1;
	} catch (const runtime_error_code& error) {
		if (error.value != 201) {
			return 3;
		}
	}

	try {
		(void)::u_system::p_round(std::numeric_limits<::u_system::t_extended>::quiet_NaN());
		return 2;
	} catch (const runtime_error_code& error) {
		if (error.value != 201) {
			return 4;
		}
	}

	return 0;
}
