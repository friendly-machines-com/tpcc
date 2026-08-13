#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/generic_intrinsics.cc" tests/generic_intrinsics.pp
tpcc_build "$tmp/generic_intrinsics" \
	"$tmp/generic_intrinsics.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/generic_intrinsics"

source=tests/generic_intrinsic_nonordinal.pp
if tpcc_translate -o"$tmp/rejected.cc" "$source" >"$tmp/stdout" 2>"$tmp/stderr"; then
	echo "expected tpcc to reject $source" >&2
	exit 1
fi
expected=$(sed -n '1p' tests/generic_intrinsic_nonordinal.error)
if ! grep -F -q -- "$expected" "$tmp/stderr"; then
	echo "wrong generic-intrinsic diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "generic-intrinsic tests passed"
