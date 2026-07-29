#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-delete-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/delete_builtin.cc" tests/delete_builtin.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/delete_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/delete_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/delete_builtin"

if ./mp -Furtl -o"$tmp/rejected.cc" \
	tests/delete_non_string_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "Delete accepted a non-ShortString generic destination" >&2
	exit 1
fi
if ! rg -Fq "no matching overload for 'delete'" "$tmp/stderr"; then
	echo "Delete produced the wrong non-ShortString diagnostic" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "Delete builtin tests passed"
