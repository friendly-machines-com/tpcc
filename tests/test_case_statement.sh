#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-case-statement-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/case_statement.cc" tests/case_statement.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/case_statement.cc" \
	"$tmp/system.cc" \
	-o "$tmp/case_statement"
ASAN_OPTIONS=detect_leaks=1 "$tmp/case_statement"

echo "case-statement tests passed"
