#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/based_integer_literals.cc" \
	tests/based_integer_literals.pp

tpcc_build "$tmp/based_integer_literals" \
	"$tmp/based_integer_literals.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/based_integer_literals"

echo "based integer literal tests passed"
