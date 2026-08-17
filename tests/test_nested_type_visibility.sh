#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/nested_type_visibility.cc" \
	tests/nested_type_visibility.pp

tpcc_build "$tmp/nested_type_visibility" \
	"$tmp/nested_type_visibility.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/nested_type_visibility"

echo "nested type visibility tests passed"
