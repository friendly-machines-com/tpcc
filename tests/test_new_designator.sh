#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/new_designator.cc" tests/new_designator.pp

tpcc_build "$tmp/new_designator" \
	tests/new_designator_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/new_designator"

echo "new-designator tests passed"
