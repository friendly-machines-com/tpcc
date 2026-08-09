#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-overflow-constants.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/unchecked.cc" \
	tests/overflow_constant_unchecked.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/unchecked.cc" \
	"$tmp/system.cc" \
	-o "$tmp/unchecked"
ASAN_OPTIONS=detect_leaks=1 "$tmp/unchecked"

if ./mp -Furtl -o"$tmp/checked.cc" \
	tests/overflow_constant_checked_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "checked overflowing constant unexpectedly compiled" >&2
	exit 1
fi
rg -Fq 'integer constant out of range for target type' \
	"$tmp/stderr"

echo "overflow constant-folding tests passed"
