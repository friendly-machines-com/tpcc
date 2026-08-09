#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-h-directive-strings-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/h_directive_strings.cc" \
	tests/h_directive_strings.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/h_directive_strings.cc" \
	"$tmp/system.cc" \
	-o "$tmp/h_directive_strings"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/h_directive_strings"

if ./mp -Furtl -o"$tmp/longstrings_invalid.cc" \
	tests/longstrings_invalid.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted invalid LONGSTRINGS setting" >&2
	exit 1
fi
if ! rg -Fq \
	"$(sed -n '1p' tests/longstrings_invalid.error)" \
	"$tmp/stderr"
then
	echo "wrong LONGSTRINGS diagnostic" >&2
	sed -n '1,40p' "$tmp/stderr" >&2
	exit 1
fi

echo "H directive string tests passed"
