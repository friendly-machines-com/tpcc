#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/function_result_designator.cc" \
	tests/function_result_designator.pp

tpcc_build "$tmp/function_result_designator" \
	"$tmp/function_result_designator.cc" \
	"$tmp/system.cc"

tpcc_run "$tmp/function_result_designator"

echo "function-result designator test passed"
