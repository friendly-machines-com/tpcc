#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/custom_checked_operators.cc" \
	tests/custom_checked_operators.pp

grep -Fq 'o_unchecked_add' \
	"$tmp/custom_checked_operators.cc"
grep -Fq 'o_add' \
	"$tmp/custom_checked_operators.cc"
grep -Fq 'o_operator_plus' \
	"$tmp/custom_checked_operators.cc"
grep -Fq 'p_add' \
	"$tmp/custom_checked_operators.cc"

tpcc_build "$tmp/custom_checked_operators" \
	"$tmp/custom_checked_operators.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/custom_checked_operators"

echo "custom checked-operator tests passed"
