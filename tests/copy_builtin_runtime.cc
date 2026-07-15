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
	return value.length == length &&
	       std::memcmp(value.data, bytes, length) == 0 &&
	       value.data[length] == 0;
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
	auto assigned = ::u_system::p_assign(
	    ::u_system::tpcc_shortstring_from_c("abc", strlen("abc")));
	if (!equals(assigned, "abc", 3))
		return 8;
	::u_system::p_setlength(assigned, 5);
	if (assigned.length != 5 ||
	    assigned.data[3] != 0 ||
	    assigned.data[4] != 0 ||
	    assigned.data[5] != 0)
		return 9;
	::u_system::p_setlength(assigned, -1);
	if (assigned.length != 0 || assigned.data[0] != 0)
		return 10;
	::u_system::p_setlength(assigned, 1000);
	if (assigned.length != 254 || assigned.data[254] != 0)
		return 11;
	return 0;
}
