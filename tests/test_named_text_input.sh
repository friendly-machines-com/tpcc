#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-named-text-input-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/named_text_input.cc" \
	tests/named_text_input.pp

{
	printf 'alpha\r\n'
	awk 'BEGIN { for (i = 0; i < 300; ++i) printf "x"; printf "\n" }'
	printf 'omega\r'
} >"$tmp/input.txt"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/named_text_input.cc" \
	"$tmp/system.cc" \
	-o "$tmp/named_text_input"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/named_text_input" "$tmp/input.txt"

echo "named Text input tests passed"
