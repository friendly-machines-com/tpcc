#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-val-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/val_builtin.cc" tests/val_builtin.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/val_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/val_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/val_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/val_builtin_runtime.cpp \
	-o "$tmp/val_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/val_builtin"

echo "Val builtin tests passed"
