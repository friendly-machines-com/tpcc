#!/bin/sh
set -eu

tmp=${TMPDIR:-/tmp}/tpcc-operator-catalog.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

${CXX:-c++} -std=c++20 -Wall -Wextra \
	tests/operator_catalog.cc src/operators.cc \
	-o "$tmp/operator_catalog"
"$tmp/operator_catalog"

if ./mp -Furtl \
	-o"$tmp/operator_lifecycle_unsupported.cc" \
	tests/operator_lifecycle_unsupported.pp \
	>"$tmp/lifecycle.out" 2>&1
then
	echo "accepted unsupported lifecycle operator" >&2
	exit 1
fi

expected=$(sed -n '1p' tests/operator_lifecycle_unsupported.error)
if ! rg -Fq "$expected" "$tmp/lifecycle.out"
then
	echo "wrong unsupported lifecycle diagnostic; expected:" >&2
	echo "$expected" >&2
	cat "$tmp/lifecycle.out" >&2
	exit 1
fi

if ./mp -Furtl \
	-o"$tmp/operator_not_equal_unsupported.cc" \
	tests/operator_not_equal_unsupported.pp \
	>"$tmp/not_equal.out" 2>&1
then
	echo "accepted a separately declarable NotEqual operator" >&2
	exit 1
fi

if ! rg -Fq "unknown custom operator 'notequal'" \
	"$tmp/not_equal.out"
then
	echo "wrong NotEqual diagnostic" >&2
	cat "$tmp/not_equal.out" >&2
	exit 1
fi

echo "operator catalog tests passed"
