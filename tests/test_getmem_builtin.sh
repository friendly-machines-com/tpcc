#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-getmem-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/getmem_builtin.cc" tests/getmem_builtin.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/getmem_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/getmem_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/getmem_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/getmem_builtin_runtime.cc \
	-o "$tmp/getmem_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/getmem_builtin"

echo "GetMem/AllocMem/ReAllocMem/FreeMem builtin tests passed"
