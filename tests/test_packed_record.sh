#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-packed-record-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/23_packed_record.cc" tests/23_packed_record.pp

diff -u tests/23_packed_record.cc "$tmp/23_packed_record.cc"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/23_packed_record.cc" \
	"$tmp/system.cc" \
	-o "$tmp/23_packed_record"
ASAN_OPTIONS=detect_leaks=1 "$tmp/23_packed_record"

./mp -Furtl -o"$tmp/packed_overlay.cc" tests/packed_overlay.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/packed_overlay.cc" \
	"$tmp/system.cc" \
	-o "$tmp/packed_overlay"
ASAN_OPTIONS=detect_leaks=1 "$tmp/packed_overlay"

./mp -Furtl \
	-o"$tmp/packed_array_field_write.cc" \
	tests/packed_array_field_write.pp
for required in \
	'auto tpcc_packed_field = tpcc_packed_value.m_get_p_data();' \
	'static_assert(alignof(tpcc_packed_item_type) == 1' \
	'::u_system::p_index(tpcc_packed_field,' \
	'tpcc_packed_value.m_set_p_data(tpcc_packed_field);'
do
	if ! rg -Fq "$required" "$tmp/packed_array_field_write.cc"
	then
		echo "missing indexed packed-field copyback: $required" >&2
		exit 1
	fi
done
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/packed_array_field_write.cc" \
	"$tmp/system.cc" \
	-o "$tmp/packed_array_field_write"
ASAN_OPTIONS=detect_leaks=1 "$tmp/packed_array_field_write"

./mp -Furtl -o"$tmp/packed_variant.cc" tests/packed_variant.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/packed_variant.cc" \
	"$tmp/system.cc" \
	-o "$tmp/packed_variant"
ASAN_OPTIONS=detect_leaks=1 "$tmp/packed_variant"

for source in tests/packed_rejected/*.pp; do
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

echo "packed-record tests passed"
