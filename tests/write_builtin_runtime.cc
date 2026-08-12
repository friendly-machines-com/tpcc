#include <sstream>

class tracking_stringbuf : public std::stringbuf {
public:
	int sync_count = 0;

	int sync() override {
		++sync_count;
		return std::stringbuf::sync();
	}
};

#define main tpcc_pascal_main
#include "write_builtin.cc"
#undef main

int main() {
	tracking_stringbuf standard_output;
	tracking_stringbuf standard_error;
	tracking_stringbuf file_buffer;
	std::ostream file_output{&file_buffer};
	std::streambuf* old_output =
	    std::cout.rdbuf(&standard_output);
	std::streambuf* old_error =
	    std::cerr.rdbuf(&standard_error);
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
	// cerr is tied to cout, so later stderr operations may add cout
	// synchronizations after the explicit Flush(stdout).
	if (standard_output.sync_count < 1)
		return 7;
	// cerr may synchronize after each output operation because it retains
	// unitbuf, but the explicit Flush must leave it synchronized as well.
	if (standard_error.sync_count < 1)
		return 8;
	if (file_buffer.str() != "file=-7:2.50\n")
		return 3;
	if (file_buffer.sync_count != 1)
		return 9;

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
	::u_system::m_unchecked_flush(failed_text);
	if (::u_system::p_ioresult() != 101)
		return 10;

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
	::u_system::m_unchecked_flush(throwing_text);
	if (::u_system::p_ioresult() != 101)
		return 11;
	return 0;
}
