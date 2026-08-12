#include <sstream>

#define main tpcc_pascal_main
#include "write_builtin.cc"
#undef main

int main() {
	std::ostringstream standard_output;
	std::ostringstream standard_error;
	std::ostringstream file_output;
	std::streambuf* old_output =
	    std::cout.rdbuf(standard_output.rdbuf());
	std::streambuf* old_error =
	    std::cerr.rdbuf(standard_error.rdbuf());
	p_destination.stream = &file_output;

	int result = tpcc_pascal_main(0, nullptr);
	std::cout.rdbuf(old_output);
	std::cerr.rdbuf(old_error);

	if (result != 0)
		return 1;
	if (standard_output.str() !=
	    "A12 Z!\n\nTRUE\n  12\nexplicit stdout")
		return 2;
	if (standard_error.str() != "explicit stderr\n")
		return 6;
	if (file_output.str() != "file=-7:2.50\n")
		return 3;

	// An opened Text whose C++ stream has failed is an old-style I/O error,
	// not a successful Write merely because no exception was configured.
	std::ostringstream failed_output;
	failed_output.setstate(std::ios::badbit);
	::u_system::t_text failed_text{
	    &failed_output};
	::u_system::m_unchecked_write(
	    failed_text,
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_integer>(7)));
	if (::u_system::p_ioresult() != 101)
		return 4;

	// Streams may also be configured to throw on failure. The unchecked
	// operation must translate that implementation mechanism into the same
	// Pascal IOResult channel rather than leaking a C++ exception.
	std::ostringstream throwing_output;
	throwing_output.setstate(
	    std::ios::badbit);
	try {
		throwing_output.exceptions(
		    std::ios::badbit);
	} catch (const std::ios_base::failure&) {
	}
	::u_system::t_text throwing_text{
	    &throwing_output};
	::u_system::m_unchecked_writeln(
	    throwing_text,
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_integer>(8)));
	if (::u_system::p_ioresult() != 101)
		return 5;
	return 0;
}
