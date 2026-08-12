#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-str-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/str_builtin.cc" tests/str_builtin.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/str_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/str_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/str_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/str_builtin_runtime.cpp \
	-o "$tmp/str_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/str_builtin"

echo "Str builtin tests passed"
