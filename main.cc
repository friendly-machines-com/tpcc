#include <cassert>
#include "parser.h"

int main(int argc, char* argv[]) {
	Parser p;
	FILE* input_file = fopen(argv[1], "r");
	assert(input_file);
	p.push_input_file(input_file, argv[1], 1);
	p.start();
	p.parse_program_or_unit();
}
