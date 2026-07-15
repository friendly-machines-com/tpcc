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
	-Werror=address-of-packed-member \
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
	-Werror=address-of-packed-member \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/packed_overlay.cc" \
	"$tmp/system.cc" \
	-o "$tmp/packed_overlay"
ASAN_OPTIONS=detect_leaks=1 "$tmp/packed_overlay"

./mp -Furtl -o"$tmp/packed_variant.cc" tests/packed_variant.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Werror=address-of-packed-member \
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
