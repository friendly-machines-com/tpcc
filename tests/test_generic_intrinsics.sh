#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-generic-intrinsics-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/generic_intrinsics.cc" tests/generic_intrinsics.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/generic_intrinsics.cc" \
	rtl/system.cc \
	-o "$tmp/generic_intrinsics"
ASAN_OPTIONS=detect_leaks=1 "$tmp/generic_intrinsics"

source=tests/generic_intrinsic_nonordinal.pp
if ./mp -Furtl -o"$tmp/rejected.cc" "$source" >"$tmp/stdout" 2>"$tmp/stderr"; then
	echo "expected tpcc to reject $source" >&2
	exit 1
fi
expected=$(sed -n '1p' tests/generic_intrinsic_nonordinal.error)
if ! rg -F -q -- "$expected" "$tmp/stderr"; then
	echo "wrong generic-intrinsic diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "generic-intrinsic tests passed"
