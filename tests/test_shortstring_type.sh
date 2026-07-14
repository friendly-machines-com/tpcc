#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-shortstring-type-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/shortstring_type.cc" \
	tests/shortstring_type.pp

for expected in \
	'pas::t_shortstring<2> p_tiny;' \
	'pas::t_shortstring<5> p_name;' \
	'pas::t_shortstring<255> p_ordinary;' \
	'pas::tpcc_shortstring_from_c<2>' \
	'pas::tpcc_shortstring_from_c<5>' \
	'pas::tpcc_shortstring_cast<2>' \
	'pas::tpcc_shortstring_cast<5>' \
	'sizeof(pas::t_shortstring<2>)' \
	'sizeof(pas::t_shortstring<5>)' \
	'sizeof(pas::t_shortstring<255>)'
do
	if ! rg -Fq "$expected" "$tmp/shortstring_type.cc"; then
		echo "missing ShortString lowering: $expected" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/shortstring_type.cc" \
	rtl/system.cc \
	-o "$tmp/shortstring_type_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/shortstring_type_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	tests/shortstring_type_runtime.cc \
	-o "$tmp/shortstring_type_runtime"
ASAN_OPTIONS=detect_leaks=1 "$tmp/shortstring_type_runtime"

echo "ShortString type tests passed"
