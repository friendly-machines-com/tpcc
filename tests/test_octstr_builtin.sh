#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/octstr_builtin.cc" tests/octstr_builtin.pp

tpcc_build "$tmp/octstr_builtin_pascal" \
	"$tmp/octstr_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/octstr_builtin_pascal"

tpcc_build "$tmp/octstr_builtin" \
	tests/octstr_builtin_runtime.cpp
tpcc_run "$tmp/octstr_builtin"

echo "OctStr builtin tests passed"
