#include "rtl.h"

#include <cstdlib>
#include <stdexcept>

int main() {
	::u_system::t_shortstring<255> abc =
	    ::u_system::tpcc_shortstring_from_c("abc", 3);
	::u_system::t_shortstring<255> abd =
	    ::u_system::tpcc_shortstring_from_c("abd", 3);

	const auto abc_storage =
	    ::u_system::tpcc_make_const_storage_ref(abc, 1);
	const auto abd_storage =
	    ::u_system::tpcc_make_const_storage_ref(abd, 1);

	if (::u_system::p_comparechar(abc_storage, abc_storage, 3) != 0)
		return EXIT_FAILURE;
	if (::u_system::p_comparechar(abc_storage, abd_storage, 3) != -1)
		return EXIT_FAILURE;
	if (::u_system::p_comparechar(abd_storage, abc_storage, 3) != 1)
		return EXIT_FAILURE;
	if (::u_system::p_comparebyte(abc_storage, abd_storage, 3) != -1)
		return EXIT_FAILURE;
	if (::u_system::p_comparechar(abc_storage, abd_storage, 0) != 0)
		return EXIT_FAILURE;
	if (::u_system::p_comparechar(abc_storage, abd_storage, -1) != 0)
		return EXIT_FAILURE;

	::u_system::t_fixedarray<::u_system::t_byte, 3, 0> with_zero_a{{1, 0, 2}};
	::u_system::t_fixedarray<::u_system::t_byte, 3, 0> with_zero_b{{1, 0, 3}};
	if (::u_system::p_comparebyte(
	        ::u_system::tpcc_make_const_storage_ref(with_zero_a, 0),
	        ::u_system::tpcc_make_const_storage_ref(with_zero_b, 0),
	        3) != -1)
		return EXIT_FAILURE;

	bool rejected = false;
	try {
		::u_system::p_comparechar(
		    ::u_system::tpcc_make_const_storage_ref(abc, 255),
		    abd_storage,
		    2);
	} catch (const std::out_of_range&) {
		rejected = true;
	}
	if (!rejected)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
