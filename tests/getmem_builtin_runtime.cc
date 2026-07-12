#include "rtl.h"

#include <cstdlib>

int main() {
	pas::t_char* characters = nullptr;
	pas::p_getmem(characters, 8);
	if (!characters)
		return EXIT_FAILURE;
	characters[0] = pas::t_char{'a'};
	characters[1] = pas::t_char{0};
	if (pas::p_freemem(characters) != 0)
		return EXIT_FAILURE;

	pas::t_pointer raw = pas::p_getmem(8);
	if (!raw)
		return EXIT_FAILURE;
	pas::p_freemem(raw, 8);

	return EXIT_SUCCESS;
}
