#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/chr_builtin.cc" tests/chr_builtin.pp
tpcc_build "$tmp/chr_builtin" \
	tests/chr_builtin_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/chr_builtin"

echo "Chr builtin tests passed"
