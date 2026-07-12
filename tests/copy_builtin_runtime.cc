#define main tpcc_pascal_main
#include "copy_builtin.cc"
#undef main

static bool equals(const pas::t_shortstring& value, const char* bytes, std::size_t length) {
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
	return 0;
}
