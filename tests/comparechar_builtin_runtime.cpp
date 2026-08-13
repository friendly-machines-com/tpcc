#include "rtl.h"

#include <cstdlib>

struct runtime_error_code {
	::u_system::t_longint value;
};

static void raise_runtime_error(::u_system::t_longint value, ::u_system::t_pointer, ::u_system::t_pointer) {
	throw runtime_error_code{value};
}

int main() {
	::u_system::p_errorproc = &raise_runtime_error;
	::u_system::t_shortstring<255> abc = ::u_system::tpcc_shortstring_from_c("abc", 3);
	::u_system::t_shortstring<255> abd = ::u_system::tpcc_shortstring_from_c("abd", 3);

	const auto abc_storage = ::u_system::tpcc_make_const_storage_ref(abc, 1);
	const auto abd_storage = ::u_system::tpcc_make_const_storage_ref(abd, 1);

	if (::u_system::p_comparechar(abc_storage, abc_storage, 3) != 0) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_comparechar(abc_storage, abd_storage, 3) != -1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_comparechar(abd_storage, abc_storage, 3) != 1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_comparebyte(abc_storage, abd_storage, 3) != -1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_comparechar(abc_storage, abd_storage, 0) != 0) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_comparechar(abc_storage, abd_storage, -1) != 0) {
		return EXIT_FAILURE;
	}

	::u_system::t_fixedarray<::u_system::t_byte, 4, 0> bytes{{7, 9, 7, 0}};
	const auto byte_storage =
	    ::u_system::tpcc_make_const_storage_ref(bytes, 0);
	if (::u_system::p_indexbyte(byte_storage, 4, 9) != 1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_indexbyte(byte_storage, 2, 7) != 0) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_indexbyte(byte_storage, 4, 8) != -1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_indexbyte(byte_storage, 0, 7) != -1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_indexbyte(byte_storage, -1, 0) != 3) {
		return EXIT_FAILURE;
	}

	::u_system::t_fixedarray<::u_system::t_word, 3, 0> words{{
	    static_cast<::u_system::t_word>(0x1234),
	    static_cast<::u_system::t_word>(0x5678),
	    static_cast<::u_system::t_word>(0x1234),
	}};
	const auto word_storage =
	    ::u_system::tpcc_make_const_storage_ref(words, 0);
	if (::u_system::p_indexword(
	        word_storage, 3,
	        static_cast<::u_system::t_word>(0x5678)) != 1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_indexword(
	        word_storage, 1,
	        static_cast<::u_system::t_word>(0x5678)) != -1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_indexword(
	        word_storage, -1,
	        static_cast<::u_system::t_word>(0x1234)) != 0) {
		return EXIT_FAILURE;
	}

	::u_system::t_fixedarray<::u_system::t_word, 3, 0> other_words{{
	    static_cast<::u_system::t_word>(0x1234),
	    static_cast<::u_system::t_word>(0x5679),
	    static_cast<::u_system::t_word>(0x1234),
	}};
	const auto other_word_storage =
	    ::u_system::tpcc_make_const_storage_ref(other_words, 0);
	if (::u_system::p_compareword(
	        word_storage, word_storage, 3) != 0) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_compareword(
	        word_storage, other_word_storage, 3) != -1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_compareword(
	        other_word_storage, word_storage, 3) != 1) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_compareword(
	        word_storage, other_word_storage, 1) != 0) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_compareword(
	        word_storage, other_word_storage, 0) != 0) {
		return EXIT_FAILURE;
	}
	if (::u_system::p_compareword(
	        word_storage, other_word_storage, -1) != 0) {
		return EXIT_FAILURE;
	}

	::u_system::t_fixedarray<::u_system::t_byte, 3, 0> unaligned_words{};
	const ::u_system::t_word unaligned_needle =
	    static_cast<::u_system::t_word>(0xabcd);
	std::memcpy(
	    std::addressof(unaligned_words.items[1]),
	    std::addressof(unaligned_needle),
	    sizeof(unaligned_needle));
	if (::u_system::p_indexword(
	        ::u_system::tpcc_make_const_storage_ref(unaligned_words, 1),
	        1, unaligned_needle) != 0) {
		return EXIT_FAILURE;
	}
	::u_system::t_fixedarray<::u_system::t_byte, 3, 0>
	    other_unaligned_words{};
	const ::u_system::t_word larger_unaligned_word =
	    static_cast<::u_system::t_word>(0xabce);
	std::memcpy(
	    std::addressof(other_unaligned_words.items[1]),
	    std::addressof(larger_unaligned_word),
	    sizeof(larger_unaligned_word));
	if (::u_system::p_compareword(
	        ::u_system::tpcc_make_const_storage_ref(unaligned_words, 1),
	        ::u_system::tpcc_make_const_storage_ref(
	            other_unaligned_words, 1),
	        1) != -1) {
		return EXIT_FAILURE;
	}

	::u_system::t_fixedarray<::u_system::t_byte, 3, 0> with_zero_a{{1, 0, 2}};
	::u_system::t_fixedarray<::u_system::t_byte, 3, 0> with_zero_b{{1, 0, 3}};
	if (::u_system::p_comparebyte(::u_system::tpcc_make_const_storage_ref(with_zero_a, 0), ::u_system::tpcc_make_const_storage_ref(with_zero_b, 0), 3) != -1) {
		return EXIT_FAILURE;
	}

	bool rejected = false;
	try {
		::u_system::p_comparechar(::u_system::tpcc_make_const_storage_ref(abc, 255), abd_storage, 2);
	} catch (const runtime_error_code& error) {
		rejected = error.value == 201;
	}
	if (!rejected) {
		return EXIT_FAILURE;
	}

	rejected = false;
	try {
		::u_system::p_indexbyte(byte_storage, 5, 7);
	} catch (const runtime_error_code& error) {
		rejected = error.value == 201;
	}
	if (!rejected) {
		return EXIT_FAILURE;
	}

	rejected = false;
	try {
		::u_system::p_indexword(word_storage, 4, 0);
	} catch (const runtime_error_code& error) {
		rejected = error.value == 201;
	}
	if (!rejected) {
		return EXIT_FAILURE;
	}

	rejected = false;
	try {
		::u_system::p_compareword(
		    word_storage,
		    ::u_system::tpcc_make_const_storage_ref(words, 1),
		    3);
	} catch (const runtime_error_code& error) {
		rejected = error.value == 201;
	}
	if (!rejected) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
