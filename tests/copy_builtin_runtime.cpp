#define main tpcc_pascal_main
#include "copy_builtin.cc"
#undef main

template<std::size_t Capacity>
static bool equals(
    const ::u_system::t_shortstring<Capacity>& value,
    const char* bytes,
    std::size_t length) {
	return value.length == length &&
	       std::memcmp(value.data, bytes, length) == 0;
}

static bool equals(
    const ::u_system::t_ansistring& value,
    const char* bytes,
    std::size_t length) {
	return value.m_length() ==
		   static_cast<::u_system::t_sizeint>(length) &&
	       std::memcmp(value.m_data(), bytes, length) == 0 &&
	       value.m_data()[length] == 0;
}

int main() {
	if (tpcc_pascal_main() != 0)
		return 1;
	if (!equals(p_middle, "bcd", 3))
		return 2;
	if (!equals(p_tail, "ef", 2))
		return 3;
	if (!equals(p_missing, "", 0))
		return 4;
	const char embedded[] = {'\0', 'b'};
	if (!equals(p_embedded, embedded, sizeof(embedded)))
		return 5;
	if (!equals(p_one, "x", 1))
		return 6;
	if (!equals(p_trimmed, "abc", 3))
		return 7;
	auto assigned = ::u_system::o_implicit(
	    ::u_system::tpcc_shortstring_from_c(
	        "abc", strlen("abc")),
	    ::u_system::m_conversion_target<
	        ::u_system::t_ansistring>{});
	if (!equals(assigned, "abc", 3))
		return 8;
	::u_system::p_setlength(assigned, 5);
	if (assigned.m_length() != 5 ||
	    assigned.m_data()[3] != 0 ||
	    assigned.m_data()[4] != 0 ||
	    assigned.m_data()[5] != 0)
		return 9;
	::u_system::p_setlength(assigned, -1);
	if (assigned.m_length() != 0 ||
	    assigned.m_data()[0] != 0)
		return 10;
	::u_system::p_setlength(assigned, 1000);
	if (assigned.m_length() != 1000 ||
	    assigned.m_data()[1000] != 0)
		return 11;
	return 0;
}
