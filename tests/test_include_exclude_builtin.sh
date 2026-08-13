#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/include_exclude_builtin.cc" \
	tests/include_exclude_builtin.pp

if ! grep -Fq '::u_system::p_include(' "$tmp/include_exclude_builtin.cc"; then
	echo "Include did not lower through the RTL" >&2
	exit 1
fi
if ! grep -Fq '::u_system::p_exclude(' "$tmp/include_exclude_builtin.cc"; then
	echo "Exclude did not lower through the RTL" >&2
	exit 1
fi

tpcc_build "$tmp/include_exclude_builtin" \
	tests/include_exclude_builtin_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/include_exclude_builtin"

echo "Include/Exclude builtin tests passed"
