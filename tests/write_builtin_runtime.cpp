#include <cstdio>
#include <string>

#define TPCC_TEST_GENERATED_PROGRAM "write_builtin.cc"
#include "generated_program_runtime.h"

static std::string read_output(std::FILE* file) {
	if (std::fflush(file) != 0) {
		return {};
	}
	if (std::fseek(file, 0, SEEK_END) != 0) {
		return {};
	}
	const long length = std::ftell(file);
	if (length < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
		return {};
	}
	std::string result(static_cast<std::size_t>(length), '\0');
	if (!result.empty() && std::fread(result.data(), 1, result.size(), file) != result.size()) {
		return {};
	}
	return result;
}

int main() {
	std::FILE* standard_output = std::tmpfile();
	std::FILE* standard_error = std::tmpfile();
	std::FILE* file_output = std::tmpfile();
	if (!standard_output || !standard_error || !file_output) {
		return 1;
	}

	std::FILE* old_output = ::u_system::m_stdout_text_state.handle;
	std::FILE* old_error = ::u_system::m_stderr_text_state.handle;
	::u_system::m_stdout_text_state.handle = standard_output;
	::u_system::m_stderr_text_state.handle = standard_error;
	p_destination.state = new ::u_system::text_file_state{
	    .name = {},
	    .handle = file_output,
	    .mode = ::u_system::text_file_mode::Output,
	    .standard_stream = false,
	};

	const int result = tpcc_run_generated_program();
	::u_system::m_stdout_text_state.handle = old_output;
	::u_system::m_stderr_text_state.handle = old_error;

	if (result != 0) {
		return 2;
	}
	std::string expected_output =
	    "A12 Z!\n\nTRUE\n  12\n  pointer\n\n";
	expected_output.append("ab\0cd", 5);
	expected_output.append("\nexplicit stdout");
	if (read_output(standard_output) != expected_output) {
		return 3;
	}
	if (read_output(standard_error) != "explicit stderr\n") {
		return 4;
	}
	if (read_output(file_output) != "file=-7:2.50\n") {
		return 5;
	}

	::u_system::m_release_text_file_state(p_destination.state);
	std::fclose(standard_output);
	std::fclose(standard_error);

	// A Text in output mode whose FILE rejects writes reports the ordinary
	// Pascal I/O status. The host mechanism remains entirely inside stdio.
	std::FILE* failed_output = std::fopen("/dev/full", "wb");
	if (!failed_output) {
		return 6;
	}
	std::setvbuf(failed_output, nullptr, _IONBF, 0);
	::u_system::t_text failed_text{new ::u_system::text_file_state{
	    .name = {},
	    .handle = failed_output,
	    .mode = ::u_system::text_file_mode::Output,
	    .standard_stream = false,
	}};
	::u_system::m_unchecked_write(failed_text, ::u_system::tpcc_make_formatted_value(static_cast<::u_system::t_integer>(7)));
	if (::u_system::p_ioresult() != 101) {
		return 7;
	}

	return 0;
}
