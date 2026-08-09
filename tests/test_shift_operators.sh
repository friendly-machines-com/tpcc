#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-shift-operators.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/shift_operators.cc" tests/shift_operators.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/shift_operators.cc" \
	"$tmp/system.cc" \
	-o "$tmp/shift_operators"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/shift_operators"

echo "shift operator tests passed"
