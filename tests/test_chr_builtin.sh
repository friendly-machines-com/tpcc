#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-chr-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/chr_builtin.cc" tests/chr_builtin.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	tests/chr_builtin_runtime.cc \
	rtl/system.cc \
	-o "$tmp/chr_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/chr_builtin"

echo "Chr builtin tests passed"
