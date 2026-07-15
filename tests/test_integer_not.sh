#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-integer-not-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/integer_not.cc" tests/integer_not.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/integer_not.cc" \
	"$tmp/system.cc" \
	-o "$tmp/integer_not"
ASAN_OPTIONS=detect_leaks=1 "$tmp/integer_not"

echo "integer not tests passed"
