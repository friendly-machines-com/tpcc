#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/shift_operators.cc" tests/shift_operators.pp

tpcc_build "$tmp/shift_operators" \
	"$tmp/shift_operators.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/shift_operators"

echo "shift operator tests passed"
