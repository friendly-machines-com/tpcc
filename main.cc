#include <cassert>
#include "parser.h"
#include "units.h"

int main(int argc, char* argv[]) {
	UnitRegistry registry;
	Parser p(&registry);
	FILE* input_file = fopen(argv[1], "r");
	assert(input_file);
	p.push_input_file(input_file, argv[1], 1);
	p.start();
	p.parse_program_or_unit();
}
