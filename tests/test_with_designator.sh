#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/with_designator.cc" \
	tests/with_designator.pp

tpcc_build "$tmp/with_designator" \
	"$tmp/with_designator.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/with_designator"

echo "with-designator tests passed"
