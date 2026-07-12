#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-pos-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/pos_builtin.cc" tests/pos_builtin.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/pos_builtin.cc" \
	rtl/system.cc \
	-o "$tmp/pos_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/pos_builtin"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/pos_char_runtime.cc \
	-o "$tmp/pos_char_runtime"
ASAN_OPTIONS=detect_leaks=1 "$tmp/pos_char_runtime"

echo "Pos builtin tests passed"
