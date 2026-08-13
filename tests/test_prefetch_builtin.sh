#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/prefetch_builtin.cc" \
	tests/prefetch_builtin.pp

if ! rg -q \
	'::u_system::p_prefetch' \
	"$tmp/prefetch_builtin.cc"
then
	echo "Prefetch did not lower through the RTL" >&2
	exit 1
fi

tpcc_build "$tmp/prefetch_builtin" \
	-O2 \
	"$tmp/prefetch_builtin.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/prefetch_builtin"

echo "Prefetch builtin tests passed"
