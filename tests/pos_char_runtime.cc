#include "rtl.h"

int main() {
	auto text = pas::tpcc_shortstring_from_c(
	    "abc", strlen("abc"));
	if (pas::p_pos('b', text) != 2)
		return 1;
	if (pas::p_pos('x', text) != 0)
		return 2;
	const char embedded[] = {'a', '\0', 'b'};
	auto embedded_text =
	    pas::tpcc_shortstring_from_c(embedded, 3);
	if (embedded_text.length != 3 ||
	    embedded_text.data[0] != 'a' ||
	    embedded_text.data[1] != 0 ||
	    embedded_text.data[2] != 'b')
		return 3;
	char long_text[300]{};
	std::fill(std::begin(long_text), std::end(long_text), 'x');
	auto capped_text = pas::tpcc_shortstring_from_c(
	    long_text, sizeof(long_text));
	if (capped_text.length != decltype(capped_text)::capacity ||
	    capped_text.data[decltype(capped_text)::capacity - 1] != 'x')
		return 4;
	return 0;
}
