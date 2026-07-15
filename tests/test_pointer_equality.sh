#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-pointer-equality-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/pointer_equality.cc" tests/pointer_equality.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/pointer_equality.cc" \
	"$tmp/system.cc" \
	-o "$tmp/pointer_equality"
ASAN_OPTIONS=detect_leaks=1 "$tmp/pointer_equality"

echo "pointer equality tests passed"
