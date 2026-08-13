#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/getmem_builtin.cc" tests/getmem_builtin.pp

tpcc_build "$tmp/getmem_builtin_pascal" \
	"$tmp/getmem_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/getmem_builtin_pascal"

tpcc_build "$tmp/getmem_builtin" \
	tests/getmem_builtin_runtime.cpp
tpcc_run "$tmp/getmem_builtin"

echo "GetMem/AllocMem/ReAllocMem/FreeMem builtin tests passed"
