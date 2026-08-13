#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/math_intrinsics.cc" tests/math_intrinsics.pp
tpcc_build "$tmp/math_intrinsics" \
	"$tmp/math_intrinsics.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/math_intrinsics"

tpcc_build "$tmp/math_intrinsics_runtime_range" \
	tests/math_intrinsics_runtime_range.cpp
tpcc_run "$tmp/math_intrinsics_runtime_range"

if tpcc_translate -o"$tmp/range_error.cc" tests/math_intrinsics_range_error.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"; then
	echo "expected out-of-range folded Trunc to fail" >&2
	exit 1
fi
expected=$(sed -n '1p' tests/math_intrinsics_range_error.error)
if ! grep -F -q -- "$expected" "$tmp/stderr"; then
	echo "wrong Trunc range diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "math-intrinsic tests passed"
