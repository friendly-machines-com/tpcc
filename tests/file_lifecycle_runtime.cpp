#include "rtl.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

static bool descriptor_is_closed(int descriptor) {
	errno = 0;
	return ::fcntl(descriptor, F_GETFD) == -1 &&
	    errno == EBADF;
}

int main() {
	const auto binary_name =
	    ::u_system::tpcc_shortstring_from_c(
		"/tmp/tpcc-file-lifecycle-binary",
		sizeof("/tmp/tpcc-file-lifecycle-binary") - 1);
	int binary_descriptor = -1;
	{
		::u_system::t_file file;
		::u_system::p_assign(file, binary_name);
		::u_system::p_rewrite(file, 1);
		binary_descriptor =
		    ::fileno(file.state->handle);
		if (binary_descriptor < 0)
			return 1;
	}
	if (!descriptor_is_closed(binary_descriptor))
		return 2;

	const char* text_path =
	    "/tmp/tpcc-file-lifecycle-text";
	std::FILE* created =
	    std::fopen(text_path, "wb");
	if (!created)
		return 3;
	std::fputs("line\n", created);
	std::fclose(created);

	const auto text_name =
	    ::u_system::tpcc_shortstring_from_c(
		text_path, std::strlen(text_path));
	::u_system::t_text text;
	::u_system::p_assign(text, text_name);
	::u_system::p_reset(text);
	const int text_descriptor =
	    ::fileno(text.state->handle);
	if (text_descriptor < 0)
		return 4;

	::u_system::p_finalize(
	    ::u_system::tpcc_make_storage_ref(text));
	if (text.state != nullptr)
		return 5;
	if (!descriptor_is_closed(text_descriptor))
		return 6;

	std::remove(
	    "/tmp/tpcc-file-lifecycle-binary");
	std::remove(text_path);
	return 0;
}
