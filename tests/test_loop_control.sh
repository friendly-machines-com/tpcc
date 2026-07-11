#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-loop-control-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/loop_control.cc" tests/loop_control.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/loop_control.cc" \
	rtl/system.cc \
	-o "$tmp/loop_control"
ASAN_OPTIONS=detect_leaks=1 "$tmp/loop_control"

for source in tests/loop_break_outside.pp tests/loop_continue_outside.pp; do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" >"$tmp/stdout" 2>"$tmp/stderr"; then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! rg -F -q -- "$expected" "$tmp/stderr"; then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "loop-control tests passed"
