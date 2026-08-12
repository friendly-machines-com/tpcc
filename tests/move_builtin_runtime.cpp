#include "rtl.h"

#include <cstdlib>
struct runtime_error_code {
	::u_system::t_longint value;
};

static void raise_runtime_error(
    ::u_system::t_longint value,
    ::u_system::t_pointer,
    ::u_system::t_pointer) {
	throw runtime_error_code{value};
}

int main() {
	::u_system::p_errorproc = &raise_runtime_error;
	::u_system::t_shortstring<255> text =
	    ::u_system::tpcc_shortstring_from_c("abcdef", 6);
	::u_system::p_move(
	    ::u_system::tpcc_make_const_storage_ref(text, 1),
	    ::u_system::tpcc_make_storage_ref(text, 2),
	    4);
	if (::u_system::p_index(text, 1) != ::u_system::t_char{'a'} ||
	    ::u_system::p_index(text, 2) != ::u_system::t_char{'a'} ||
	    ::u_system::p_index(text, 3) != ::u_system::t_char{'b'} ||
	    ::u_system::p_index(text, 4) != ::u_system::t_char{'c'} ||
	    ::u_system::p_index(text, 5) != ::u_system::t_char{'d'} ||
	    ::u_system::p_index(text, 6) != ::u_system::t_char{'f'})
		return EXIT_FAILURE;

	::u_system::p_move(
	    ::u_system::tpcc_make_const_storage_ref(text, 1),
	    ::u_system::tpcc_make_storage_ref(text, 2),
	    0);
	::u_system::p_move(
	    ::u_system::tpcc_make_const_storage_ref(text, 1),
	    ::u_system::tpcc_make_storage_ref(text, 2),
	    -1);

	::u_system::t_fixedarray<::u_system::t_char, 6, 0> fixed{{
	    ::u_system::t_char{'a'},
	    ::u_system::t_char{'b'},
	    ::u_system::t_char{'c'},
	    ::u_system::t_char{'d'},
	    ::u_system::t_char{'e'},
	    ::u_system::t_char{'f'},
	}};
	::u_system::p_move(
	    ::u_system::tpcc_make_const_storage_ref(fixed, 0),
	    ::u_system::tpcc_make_storage_ref(fixed, 1),
	    5);
	if (::u_system::p_index(fixed, 0) != ::u_system::t_char{'a'} ||
	    ::u_system::p_index(fixed, 1) != ::u_system::t_char{'a'} ||
	    ::u_system::p_index(fixed, 5) != ::u_system::t_char{'e'})
		return EXIT_FAILURE;

	::u_system::t_char* pointer = nullptr;
	::u_system::p_getmem(pointer, 7);
	::u_system::p_move(
	    ::u_system::tpcc_make_const_storage_ref(text, 1),
	    ::u_system::tpcc_make_storage_ref(pointer, 0),
	    6);
	::u_system::p_index(pointer, 6) = ::u_system::t_char{0};
	if (::u_system::p_strlen(pointer) != 6)
		return EXIT_FAILURE;
	::u_system::p_freemem(pointer, 7);

	::u_system::p_move(
	    ::u_system::tpcc_make_const_storage_ref(text, 1),
	    ::u_system::tpcc_make_storage_ref(text, 255),
	    1);
	if (::u_system::p_index(text, 255) != ::u_system::t_char{'a'})
		return EXIT_FAILURE;

	bool rejected = false;
	try {
		::u_system::p_move(
		    ::u_system::tpcc_make_const_storage_ref(text, 1),
		    ::u_system::tpcc_make_storage_ref(text, 256),
		    1);
	} catch (const runtime_error_code& error) {
		rejected = error.value == 201;
	}
	if (!rejected)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
