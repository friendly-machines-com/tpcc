#include "rtl.h"

#include <cstdlib>
#include <stdexcept>

int main() {
	pas::t_shortstring<255> text =
	    pas::tpcc_shortstring_from_c("abcdef", 6);
	pas::p_move(
	    pas::tpcc_make_const_storage_ref(text, 1),
	    pas::tpcc_make_storage_ref(text, 2),
	    4);
	if (pas::p_index(text, 1) != pas::t_char{'a'} ||
	    pas::p_index(text, 2) != pas::t_char{'a'} ||
	    pas::p_index(text, 3) != pas::t_char{'b'} ||
	    pas::p_index(text, 4) != pas::t_char{'c'} ||
	    pas::p_index(text, 5) != pas::t_char{'d'} ||
	    pas::p_index(text, 6) != pas::t_char{'f'})
		return EXIT_FAILURE;

	pas::p_move(
	    pas::tpcc_make_const_storage_ref(text, 1),
	    pas::tpcc_make_storage_ref(text, 2),
	    0);
	pas::p_move(
	    pas::tpcc_make_const_storage_ref(text, 1),
	    pas::tpcc_make_storage_ref(text, 2),
	    -1);

	pas::t_fixedarray<pas::t_char, 6, 0> fixed{{
	    pas::t_char{'a'},
	    pas::t_char{'b'},
	    pas::t_char{'c'},
	    pas::t_char{'d'},
	    pas::t_char{'e'},
	    pas::t_char{'f'},
	}};
	pas::p_move(
	    pas::tpcc_make_const_storage_ref(fixed, 0),
	    pas::tpcc_make_storage_ref(fixed, 1),
	    5);
	if (pas::p_index(fixed, 0) != pas::t_char{'a'} ||
	    pas::p_index(fixed, 1) != pas::t_char{'a'} ||
	    pas::p_index(fixed, 5) != pas::t_char{'e'})
		return EXIT_FAILURE;

	pas::t_char* pointer = nullptr;
	pas::p_getmem(pointer, 7);
	pas::p_move(
	    pas::tpcc_make_const_storage_ref(text, 1),
	    pas::tpcc_make_storage_ref(pointer, 0),
	    6);
	pas::p_index(pointer, 6) = pas::t_char{0};
	if (pas::p_strlen(pointer) != 6)
		return EXIT_FAILURE;
	pas::p_freemem(pointer, 7);

	pas::p_move(
	    pas::tpcc_make_const_storage_ref(text, 1),
	    pas::tpcc_make_storage_ref(text, 255),
	    1);
	if (pas::p_index(text, 255) != pas::t_char{'a'})
		return EXIT_FAILURE;

	bool rejected = false;
	try {
		pas::p_move(
		    pas::tpcc_make_const_storage_ref(text, 1),
		    pas::tpcc_make_storage_ref(text, 256),
		    1);
	} catch (const std::out_of_range&) {
		rejected = true;
	}
	if (!rejected)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
