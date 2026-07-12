#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-properties-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/properties.cc" tests/properties.pp

if rg -q 'p_[a-zA-Z0-9_]+\[' "$tmp/properties.cc"; then
	echo "generated Pascal indexing bypassed the RTL" >&2
	exit 1
fi

if [ "$(rg -c 'pas::p_index_write\\(p_longs, 1ull\\)' "$tmp/properties.cc")" -ne 3 ]; then
	echo "AnsiString assignment, var argument, and address-of must use the uniqueness barrier" >&2
	exit 1
fi
if ! rg -q 'p_ac = pas::p_index\\(p_longs, 1ull\\)' "$tmp/properties.cc"; then
	echo "ordinary AnsiString reads must not invoke the uniqueness barrier" >&2
	exit 1
fi
if ! rg -q 'pas::p_uniquestring\\(p_longs\\)' "$tmp/properties.cc"; then
	echo "System.UniqueString must lower as an ordinary RTL call" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	tests/properties_runtime.cc \
	rtl/system.cc \
	-o "$tmp/properties"
ASAN_OPTIONS=detect_leaks=1 "$tmp/properties"

for source in \
	tests/conversion_expected_return_rejected.pp \
	tests/property_missing_indexes_rejected.pp \
	tests/property_method_reference_rejected.pp \
	tests/property_no_default_rejected.pp \
	tests/property_temporary_index_write_rejected.pp \
	tests/property_write_only_read_rejected.pp
do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" >"$tmp/stdout" 2>"$tmp/stderr"; then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! rg -F -q -- "$expected" "$tmp/stderr"; then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "property tests passed"
