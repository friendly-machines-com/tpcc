#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/val_builtin.cc" tests/val_builtin.pp

tpcc_build "$tmp/val_builtin_pascal" \
	"$tmp/val_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/val_builtin_pascal"

tpcc_build "$tmp/val_builtin" \
	tests/val_builtin_runtime.cpp
tpcc_run "$tmp/val_builtin"

echo "Val builtin tests passed"
