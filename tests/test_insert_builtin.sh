#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-insert-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/insert_builtin.cc" tests/insert_builtin.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/insert_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/insert_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/insert_builtin"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/insert_runtime.cc \
	-o "$tmp/insert_runtime"
ASAN_OPTIONS=detect_leaks=1 "$tmp/insert_runtime"

echo "Insert builtin tests passed"
