#include "emit.h"
#include "parser.h"
#include "units.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

// Split "-d<sym>[:=<val>]" or "-d<sym>[=<val>]" into (sym, val). Value is
// empty for a boolean define. FPC accepts both `:=` and `=` in this position.
static void parse_define_arg(const char* arg, std::string& sym, std::string& val) {
	const char* p = arg;
	while (*p && *p != ':' && *p != '=') {
		p++;
	}
	sym.assign(arg, p - arg);
	if (*p == ':' && p[1] == '=') {
		val.assign(p + 2);
	} else if (*p == '=') {
		val.assign(p + 1);
	} else {
		val.clear();
	}
}

static void print_help() {
	printf("Usage: mp [options] <source-file>\n");
	printf("Options:\n");
	printf("  -d<sym>[:=<val>]  Define a preprocessor symbol.\n");
	printf("  -Fu<path>         Add unit search path.\n");
	printf("  -Fi<path>         Add include-file search path.\n");
	printf("  -o<file>          Override output file name (default: source with .cc).\n");
	printf("  -h, --help        Show this help.\n");
}

int main(int argc, char* argv[]) {
	CompilerOptions options;
	std::string source_path;
	std::string output_path;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];
		if (a[0] == '-') {
			if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
				print_help();
				return 0;
			} else if (a[1] == 'd' && a[2]) {
				std::string sym, val;
				parse_define_arg(a + 2, sym, val);
				options.defines[sym] = val;
			} else if (a[1] == 'F' && a[2] == 'u' && a[3]) {
				options.unit_search_paths.emplace_back(a + 3);
			} else if (a[1] == 'F' && a[2] == 'i' && a[3]) {
				options.include_search_paths.emplace_back(a + 3);
			} else if (a[1] == 'o' && a[2]) {
				output_path = a + 2;
			} else {
				fprintf(stderr, "mp: unrecognized option '%s' (use -h for supported options)\n", a);
				return 2;
			}
		} else {
			if (!source_path.empty()) {
				fprintf(stderr, "mp: multiple source files given; only one supported\n");
				return 2;
			}
			source_path = a;
		}
	}

	if (source_path.empty()) {
		fprintf(stderr, "mp: no source file given (use -h for help)\n");
		return 2;
	}
	if (output_path.empty()) {
		output_path = derive_output_path(source_path);
	}

	auto slash = output_path.find_last_of('/');
	options.output_dir = (slash == std::string::npos) ? "" : output_path.substr(0, slash);

	UnitRegistry registry;
	Emitter emitter;
	// Don't open the emitter here -- parse_program_or_unit opens it for the
	// right shape (program .cc vs unit .h+.cc) based on the first keyword.
	options.program_output_path = output_path;
	Parser p(&registry, &emitter, &options);
	FILE* input_file = fopen(source_path.c_str(), "r");
	if (!input_file) {
		fprintf(stderr, "mp: cannot open source file: %s\n", source_path.c_str());
		return 1;
	}
	p.push_input_file(input_file, source_path, 1);
	p.start();
	p.parse_program_or_unit();
	emitter.close();
	return 0;
}
