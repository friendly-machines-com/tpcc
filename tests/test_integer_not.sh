#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/integer_not.cc" tests/integer_not.pp
tpcc_build "$tmp/integer_not" \
	"$tmp/integer_not.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/integer_not"

echo "integer not tests passed"
