#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/copy_builtin.cc" tests/copy_builtin.pp
if ! grep -Eq '::u_system::p_copy\(' "$tmp/copy_builtin.cc"; then
	echo "Copy did not lower to its ordinary RTL call" >&2
	exit 1
fi

tpcc_build "$tmp/copy_builtin" \
	tests/copy_builtin_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/copy_builtin"

echo "Copy builtin tests passed"
