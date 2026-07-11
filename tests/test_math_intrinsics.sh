#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-math-intrinsics-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/math_intrinsics.cc" tests/math_intrinsics.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/math_intrinsics.cc" \
	rtl/system.cc \
	-o "$tmp/math_intrinsics"
ASAN_OPTIONS=detect_leaks=1 "$tmp/math_intrinsics"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	tests/math_intrinsics_runtime_range.cc \
	-o "$tmp/math_intrinsics_runtime_range"
ASAN_OPTIONS=detect_leaks=1 "$tmp/math_intrinsics_runtime_range"

if ./mp -Furtl -o"$tmp/range_error.cc" tests/math_intrinsics_range_error.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"; then
	echo "expected out-of-range folded Trunc to fail" >&2
	exit 1
fi
expected=$(sed -n '1p' tests/math_intrinsics_range_error.error)
if ! rg -F -q -- "$expected" "$tmp/stderr"; then
	echo "wrong Trunc range diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "math-intrinsic tests passed"
