#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-strlen-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/strlen_builtin.cc" tests/strlen_builtin.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/strlen_builtin.cc" \
	rtl/system.cc \
	-o "$tmp/strlen_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/strlen_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/strlen_builtin_runtime.cc \
	-o "$tmp/strlen_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/strlen_builtin"

echo "StrLen builtin tests passed"
