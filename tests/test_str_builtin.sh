#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/str_builtin.cc" tests/str_builtin.pp

tpcc_build "$tmp/str_builtin_pascal" \
	"$tmp/str_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/str_builtin_pascal"

tpcc_build "$tmp/str_builtin" \
	tests/str_builtin_runtime.cpp
tpcc_run "$tmp/str_builtin"

echo "Str builtin tests passed"
