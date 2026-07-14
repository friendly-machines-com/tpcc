#include "rtl.h"

#include <cstddef>

static pas::t_shortstring<255> repeated(
    char value, std::size_t length) {
	pas::t_shortstring<255> result{};
	result.length = static_cast<uint8_t>(length);
	for (std::size_t i = 0; i < length; ++i)
		result.data[i] = value;
	result.data[length] = 0;
	return result;
}

int main() {
	auto destination = repeated('d', 250);
	auto source = repeated('s', 10);
	pas::p_insert(source, destination, 2);
	if (destination.length != 255 || destination.data[0] != 'd')
		return 1;
	for (std::size_t i = 1; i < 11; ++i)
		if (destination.data[i] != 's')
			return 2;
	for (std::size_t i = 11; i < 255; ++i)
		if (destination.data[i] != 'd')
			return 3;
	auto aliased = repeated('a', 200);
	pas::p_insert(aliased, aliased, 2);
	if (aliased.length != 255)
		return 4;
	for (std::size_t i = 0; i < 255; ++i)
		if (aliased.data[i] != 'a')
			return 5;

	auto characters = pas::tpcc_shortstring_from_c("ac", strlen("ac"));
	pas::p_insert('b', characters, 2);
	if (characters.length != 3 ||
	    characters.data[0] != 'a' ||
	    characters.data[1] != 'b' ||
	    characters.data[2] != 'c')
		return 6;

	return 0;
}
