#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-empty-statement-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/empty_statement.cc" tests/empty_statement.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/empty_statement.cc" \
	"$tmp/system.cc" \
	-o "$tmp/empty_statement"
ASAN_OPTIONS=detect_leaks=1 "$tmp/empty_statement"

echo "empty-statement tests passed"
