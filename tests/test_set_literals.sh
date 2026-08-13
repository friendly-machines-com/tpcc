#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/set_literals.cc" tests/set_literals.pp
if ! rg -q '::u_system::t_set<::u_system::t_char>' "$tmp/set_literals.cc"; then
	echo "Char set lost its item type" >&2
	exit 1
fi
if ! rg -q '::u_system::tpcc_set_range' "$tmp/set_literals.cc"; then
	echo "set range did not lower through the RTL" >&2
	exit 1
fi
if ! rg -q '::u_system::o_in' "$tmp/set_literals.cc"; then
	echo "set membership did not lower through the RTL" >&2
	exit 1
fi

tpcc_build "$tmp/set_literals" \
	tests/set_literals_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/set_literals"

echo "set literal tests passed"
