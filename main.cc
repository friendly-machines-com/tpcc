#include <cassert>
#include <string>
#include "parser.h"
#include "units.h"
#include "emit.h"

/** Given "path/to/foo.pp" produce "path/to/foo.cc"; if there's no extension
 *  (or the last dot is in a directory component), append .cc. */
static std::string derive_output_path(std::string input) {
	auto slash = input.find_last_of('/');
	auto dot = input.find_last_of('.');
	if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
		return input.substr(0, dot) + ".cc";
	}
	return input + ".cc";
}

int main(int argc, char* argv[]) {
	UnitRegistry registry;
	Emitter emitter;
	emitter.open_for_program(derive_output_path(argv[1]));
	Parser p(&registry, &emitter);
	FILE* input_file = fopen(argv[1], "r");
	assert(input_file);
	p.push_input_file(input_file, argv[1], 1);
	p.start();
	p.parse_program_or_unit();
	emitter.close();
}
