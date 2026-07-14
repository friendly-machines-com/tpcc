#include "rtl.h"

#include <cstdlib>
#include <stdexcept>

int main() {
	pas::t_shortstring<255> abc =
	    pas::tpcc_shortstring_from_c("abc", 3);
	pas::t_shortstring<255> abd =
	    pas::tpcc_shortstring_from_c("abd", 3);

	const auto abc_storage =
	    pas::tpcc_make_const_storage_ref(abc, 1);
	const auto abd_storage =
	    pas::tpcc_make_const_storage_ref(abd, 1);

	if (pas::p_comparechar(abc_storage, abc_storage, 3) != 0)
		return EXIT_FAILURE;
	if (pas::p_comparechar(abc_storage, abd_storage, 3) != -1)
		return EXIT_FAILURE;
	if (pas::p_comparechar(abd_storage, abc_storage, 3) != 1)
		return EXIT_FAILURE;
	if (pas::p_comparebyte(abc_storage, abd_storage, 3) != -1)
		return EXIT_FAILURE;
	if (pas::p_comparechar(abc_storage, abd_storage, 0) != 0)
		return EXIT_FAILURE;
	if (pas::p_comparechar(abc_storage, abd_storage, -1) != 0)
		return EXIT_FAILURE;

	pas::t_fixedarray<pas::t_byte, 3, 0> with_zero_a{{1, 0, 2}};
	pas::t_fixedarray<pas::t_byte, 3, 0> with_zero_b{{1, 0, 3}};
	if (pas::p_comparebyte(
	        pas::tpcc_make_const_storage_ref(with_zero_a, 0),
	        pas::tpcc_make_const_storage_ref(with_zero_b, 0),
	        3) != -1)
		return EXIT_FAILURE;

	bool rejected = false;
	try {
		pas::p_comparechar(
		    pas::tpcc_make_const_storage_ref(abc, 255),
		    abd_storage,
		    2);
	} catch (const std::out_of_range&) {
		rejected = true;
	}
	if (!rejected)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
