#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/move_builtin.cc" tests/move_builtin.pp

if ! rg -q '::u_system::p_move' "$tmp/move_builtin.cc"; then
	echo "Move did not lower through the RTL" >&2
	exit 1
fi

tpcc_build "$tmp/move_builtin_pascal" \
	"$tmp/move_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/move_builtin_pascal"

tpcc_build "$tmp/move_builtin" \
	tests/move_builtin_runtime.cpp
tpcc_run "$tmp/move_builtin"

echo "Move builtin tests passed"
