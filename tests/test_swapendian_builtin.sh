#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/swapendian_builtin.cc" \
	tests/swapendian_builtin.pp

if ! rg -Fq \
	'::u_system::p_swapendian(' \
	"$tmp/swapendian_builtin.cc"
then
	echo "SwapEndian calls did not use the System RTL entry point" >&2
	exit 1
fi

tpcc_build "$tmp/swapendian_builtin" \
	"$tmp/swapendian_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/swapendian_builtin"

echo "SwapEndian builtin tests passed"
