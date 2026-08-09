#include <csignal>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
	if (argc == 1)
		return 0;

	if (std::strcmp(argv[1], "string") == 0) {
		if (argc != 6)
			return 31;
		if (std::strcmp(argv[2], "alpha") != 0)
			return 32;
		if (std::strcmp(argv[3], "two words") != 0)
			return 33;
		if (std::strcmp(argv[4], "") != 0)
			return 34;
		if (std::strcmp(argv[5], "omega") != 0)
			return 35;
		return 0;
	}

	if (std::strcmp(argv[1], "array") == 0) {
		if (argc != 6)
			return 41;
		if (std::strcmp(argv[2], "") != 0)
			return 42;
		if (std::strcmp(argv[3], "two words") != 0)
			return 43;
		if (std::strcmp(argv[4], "\"quote\"") != 0)
			return 44;
		if (std::strcmp(argv[5], "back\\slash") != 0)
			return 45;
		return 0;
	}

	if (std::strcmp(argv[1], "environment") == 0) {
		const char* value =
		    std::getenv("TPCC_EXECUTE_PROCESS_INHERITED");
		return value &&
			       std::strcmp(value, "present") == 0
			   ? 0
			   : 51;
	}

	if (std::strcmp(argv[1], "exit") == 0) {
		if (argc != 3)
			return 61;
		return std::atoi(argv[2]);
	}

	if (std::strcmp(argv[1], "signal") == 0) {
		std::raise(SIGTERM);
		return 62;
	}

	return 63;
}
