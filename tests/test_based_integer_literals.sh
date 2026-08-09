#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-based-integer-literals.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/based_integer_literals.cc" \
	tests/based_integer_literals.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/based_integer_literals.cc" \
	"$tmp/system.cc" \
	-o "$tmp/based_integer_literals"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/based_integer_literals"

echo "based integer literal tests passed"
