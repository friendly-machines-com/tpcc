#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-explicit-enum-values-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/explicit_enum_values.cc" \
	tests/explicit_enum_values.pp

for expected in \
	'p_middle = 5' \
	'p_lowest = -2' \
	'p_highest = 9' \
	'p_back = 1' \
	'p_alias = 9' \
	'p_indextwo = 2, p_indexthree, p_indexfour' \
	'p_calculated = 16' \
	'p_charactera = 65'
do
	if ! rg -Fq "$expected" "$tmp/explicit_enum_values.cc"; then
		echo "missing explicit enum lowering: $expected" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/explicit_enum_values.cc" \
	"$tmp/system.cc" \
	-o "$tmp/explicit_enum_values"
ASAN_OPTIONS=detect_leaks=1 "$tmp/explicit_enum_values"

if ./mp -Furtl -o"$tmp/rejected.cc" \
	tests/explicit_enum_value_out_of_range.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an explicit enum value outside signed 32-bit range" >&2
	exit 1
fi
if ! rg -Fq \
	'explicit enum value is outside signed 32-bit range' \
	"$tmp/stderr"
then
	echo "wrong diagnostic for out-of-range explicit enum value" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "explicit enum value tests passed"
