#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-octstr-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/octstr_builtin.cc" tests/octstr_builtin.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/octstr_builtin.cc" \
	rtl/system.cc \
	-o "$tmp/octstr_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/octstr_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/octstr_builtin_runtime.cc \
	-o "$tmp/octstr_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/octstr_builtin"

echo "OctStr builtin tests passed"
