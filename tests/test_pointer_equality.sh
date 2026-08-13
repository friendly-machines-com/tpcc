#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/pointer_equality.cc" tests/pointer_equality.pp
tpcc_build "$tmp/pointer_equality" \
	"$tmp/pointer_equality.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/pointer_equality"

echo "pointer equality tests passed"
