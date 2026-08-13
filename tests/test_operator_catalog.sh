#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

tpcc_build_native "$tmp/operator_catalog" \
	tests/operator_catalog.cc src/operators.cc
"$tmp/operator_catalog"

if tpcc_translate \
	-o"$tmp/operator_lifecycle_unsupported.cc" \
	tests/operator_lifecycle_unsupported.pp \
	>"$tmp/lifecycle.out" 2>&1
then
	echo "accepted unsupported lifecycle operator" >&2
	exit 1
fi

expected=$(sed -n '1p' tests/operator_lifecycle_unsupported.error)
if ! grep -Fq "$expected" "$tmp/lifecycle.out"
then
	echo "wrong unsupported lifecycle diagnostic; expected:" >&2
	echo "$expected" >&2
	cat "$tmp/lifecycle.out" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/operator_not_equal_unsupported.cc" \
	tests/operator_not_equal_unsupported.pp \
	>"$tmp/not_equal.out" 2>&1
then
	echo "accepted a separately declarable NotEqual operator" >&2
	exit 1
fi

if ! grep -Fq "unknown custom operator 'notequal'" \
	"$tmp/not_equal.out"
then
	echo "wrong NotEqual diagnostic" >&2
	cat "$tmp/not_equal.out" >&2
	exit 1
fi

echo "operator catalog tests passed"
