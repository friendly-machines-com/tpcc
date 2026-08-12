#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-comparechar-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/comparechar_builtin.cc" tests/comparechar_builtin.pp

if ! rg -q '::u_system::p_comparechar' "$tmp/comparechar_builtin.cc"; then
	echo "CompareChar did not lower through the RTL" >&2
	exit 1
fi
if ! rg -q '::u_system::p_comparebyte' "$tmp/comparechar_builtin.cc"; then
	echo "CompareByte did not lower through the RTL" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/comparechar_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/comparechar_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/comparechar_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/comparechar_builtin_runtime.cpp \
	-o "$tmp/comparechar_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/comparechar_builtin"

echo "CompareChar/CompareByte builtin tests passed"
