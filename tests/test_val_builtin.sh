#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-val-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/val_builtin.cc" tests/val_builtin.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/val_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/val_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/val_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/val_builtin_runtime.cc \
	-o "$tmp/val_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/val_builtin"

if ./mp -Furtl -o"$tmp/subrange.cc" \
	tests/val_subrange_var_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "Val accepted a distinct subrange as an out base type" >&2
	exit 1
fi
if ! rg -Fq 'no matching overload' "$tmp/stderr"
then
	echo "wrong Val subrange out-parameter diagnostic" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "Val builtin tests passed"
