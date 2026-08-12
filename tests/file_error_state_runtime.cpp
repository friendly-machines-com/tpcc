#include "rtl.h"

#include <cstdio>

static bool consume_pascal_error() {
	return ::u_system::p_ioresult() != 0;
}

static int test_text_eof_error() {
	std::FILE* handle =
	    std::fopen("/dev/null", "wb");
	if (!handle)
		return 1;
	::u_system::t_text file{
	    new ::u_system::text_file_state{
		.name = {},
		.handle = handle,
		.mode =
		    ::u_system::text_file_mode::Input,
		.standard_stream = false,
	    }};

	(void)::u_system::m_unchecked_eof(file);
	if (!consume_pascal_error())
		return 2;
	if (std::ferror(handle))
		return 3;

	// Consuming IOResult must make a later, valid operation depend only on
	// that operation, not on stdio's indicator from the failed read.
	file.state->mode =
	    ::u_system::text_file_mode::Output;
	::u_system::m_unchecked_write(
	    file,
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_integer>(1)));
	if (::u_system::p_ioresult() != 0)
		return 4;
	return 0;
}

static int test_text_readln_error() {
	std::FILE* handle =
	    std::fopen("/dev/null", "wb");
	if (!handle)
		return 1;
	::u_system::t_text file{
	    new ::u_system::text_file_state{
		.name = {},
		.handle = handle,
		.mode =
		    ::u_system::text_file_mode::Input,
		.standard_stream = false,
	    }};
	::u_system::t_ansistring line;

	::u_system::m_unchecked_readln(
	    file, line);
	if (!consume_pascal_error())
		return 2;
	if (std::ferror(handle))
		return 3;
	return 0;
}

static int test_blockread_error() {
	std::FILE* handle =
	    std::fopen("/dev/null", "wb");
	if (!handle)
		return 1;
	::u_system::t_file file;
	file.state =
	    new ::u_system::binary_file_state{
		.name = {},
		.handle = handle,
		.record_size = 1,
		.readable = true,
		.writable = true,
	    };
	::u_system::t_byte value = 0;
	::u_system::t_longint transferred = 0;

	::u_system::m_unchecked_blockread(
	    file,
	    ::u_system::tpcc_make_storage_ref(
		value),
	    static_cast<::u_system::t_longint>(1),
	    transferred);
	if (!consume_pascal_error())
		return 2;
	if (std::ferror(handle))
		return 3;

	::u_system::m_unchecked_blockwrite(
	    file,
	    ::u_system::tpcc_make_const_storage_ref(
		value),
	    static_cast<::u_system::t_longint>(1),
	    transferred);
	if (::u_system::p_ioresult() != 0 ||
	    transferred != 1)
		return 4;
	return 0;
}

static int test_blockwrite_error() {
	std::FILE* handle =
	    std::fopen("/dev/null", "rb");
	if (!handle)
		return 1;
	::u_system::t_file file;
	file.state =
	    new ::u_system::binary_file_state{
		.name = {},
		.handle = handle,
		.record_size = 1,
		.readable = true,
		.writable = true,
	    };
	const ::u_system::t_byte value = 0;
	::u_system::t_longint transferred = 0;

	::u_system::m_unchecked_blockwrite(
	    file,
	    ::u_system::tpcc_make_const_storage_ref(
		value),
	    static_cast<::u_system::t_longint>(1),
	    transferred);
	if (!consume_pascal_error())
		return 2;
	if (std::ferror(handle))
		return 3;
	return 0;
}

static int test_write_error() {
	std::FILE* handle =
	    std::fopen("/dev/full", "wb");
	if (!handle)
		return 1;
	if (std::setvbuf(
	        handle, nullptr, _IONBF, 0) != 0) {
		std::fclose(handle);
		return 2;
	}
	::u_system::t_text file{
	    new ::u_system::text_file_state{
		.name = {},
		.handle = handle,
		.mode =
		    ::u_system::text_file_mode::Output,
		.standard_stream = false,
	    }};

	::u_system::m_unchecked_write(
	    file,
	    ::u_system::tpcc_make_formatted_value(
		static_cast<::u_system::t_integer>(1)));
	if (!consume_pascal_error())
		return 3;
	if (std::ferror(handle))
		return 4;
	return 0;
}

static int test_flush_error() {
	std::FILE* handle =
	    std::fopen("/dev/full", "wb");
	if (!handle)
		return 1;
	char buffer[BUFSIZ];
	if (std::setvbuf(
	        handle, buffer, _IOFBF,
	        sizeof(buffer)) != 0) {
		std::fclose(handle);
		return 2;
	}
	if (std::fwrite("x", 1, 1, handle) != 1) {
		std::fclose(handle);
		return 3;
	}
	::u_system::t_text file{
	    new ::u_system::text_file_state{
		.name = {},
		.handle = handle,
		.mode =
		    ::u_system::text_file_mode::Output,
		.standard_stream = false,
	    }};

	::u_system::m_unchecked_flush(file);
	if (!consume_pascal_error())
		return 4;
	if (std::ferror(handle))
		return 5;

	// fclose will retry the buffered host write. Detach first so that this
	// deliberate host-only failure cannot create a second Pascal error.
	file.state->handle = nullptr;
	std::fclose(handle);
	return 0;
}

int main() {
	const int eof_error =
	    test_text_eof_error();
	if (eof_error != 0)
		return 10 + eof_error;
	if (::u_system::p_ioresult() != 0)
		return 15;
	const int readln_error =
	    test_text_readln_error();
	if (readln_error != 0)
		return 20 + readln_error;
	if (::u_system::p_ioresult() != 0)
		return 25;
	const int blockread_error =
	    test_blockread_error();
	if (blockread_error != 0)
		return 30 + blockread_error;
	if (::u_system::p_ioresult() != 0)
		return 35;
	const int blockwrite_error =
	    test_blockwrite_error();
	if (blockwrite_error != 0)
		return 40 + blockwrite_error;
	if (::u_system::p_ioresult() != 0)
		return 45;
	const int write_error =
	    test_write_error();
	if (write_error != 0)
		return 50 + write_error;
	if (::u_system::p_ioresult() != 0)
		return 55;
	const int flush_error =
	    test_flush_error();
	if (flush_error != 0)
		return 60 + flush_error;
	if (::u_system::p_ioresult() != 0)
		return 65;
	return 0;
}
