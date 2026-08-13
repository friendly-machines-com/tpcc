#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/case_statement.cc" tests/case_statement.pp
tpcc_build "$tmp/case_statement" \
	"$tmp/case_statement.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/case_statement"

echo "case-statement tests passed"
