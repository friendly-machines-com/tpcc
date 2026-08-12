#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-include-exclude-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/include_exclude_builtin.cc" \
	tests/include_exclude_builtin.pp

if ! rg -Fq '::u_system::p_include(' "$tmp/include_exclude_builtin.cc"; then
	echo "Include did not lower through the RTL" >&2
	exit 1
fi
if ! rg -Fq '::u_system::p_exclude(' "$tmp/include_exclude_builtin.cc"; then
	echo "Exclude did not lower through the RTL" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	tests/include_exclude_builtin_runtime.cpp \
	"$tmp/system.cc" \
	-o "$tmp/include_exclude_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/include_exclude_builtin"

echo "Include/Exclude builtin tests passed"
