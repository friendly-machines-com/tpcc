#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/length_native_types.cc" tests/length_native_types.pp

tpcc_build "$tmp/length_native_types" \
	"$tmp/length_native_types.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/length_native_types"

echo "Length native-integer tests passed"
