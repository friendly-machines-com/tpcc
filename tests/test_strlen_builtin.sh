#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/strlen_builtin.cc" tests/strlen_builtin.pp

tpcc_build "$tmp/strlen_builtin_pascal" \
	"$tmp/strlen_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/strlen_builtin_pascal"

tpcc_build "$tmp/strlen_builtin" \
	tests/strlen_builtin_runtime.cpp
tpcc_run "$tmp/strlen_builtin"

echo "StrLen builtin tests passed"
