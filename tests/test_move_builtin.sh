#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-move-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/move_builtin.cc" tests/move_builtin.pp

if ! rg -q '::u_system::p_move' "$tmp/move_builtin.cc"; then
	echo "Move did not lower through the RTL" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/move_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/move_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/move_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/move_builtin_runtime.cpp \
	-o "$tmp/move_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/move_builtin"

echo "Move builtin tests passed"
