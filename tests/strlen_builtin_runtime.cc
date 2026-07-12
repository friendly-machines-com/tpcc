#include "rtl.h"

#include <cstdlib>

int main() {
	if (pas::p_strlen(nullptr) != 0)
		return EXIT_FAILURE;

	const pas::t_char text[] = {
	    pas::t_char{'a'},
	    pas::t_char{'b'},
	    pas::t_char{0},
	    pas::t_char{'c'},
	    pas::t_char{0},
	};
	if (pas::p_strlen(text) != 2)
		return EXIT_FAILURE;
	if (pas::p_strlen(text + 3) != 1)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
