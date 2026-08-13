#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/filldword_builtin.cc" \
	tests/filldword_builtin.pp

if ! grep -Eq '::u_system::p_filldword' \
	"$tmp/filldword_builtin.cc"
then
	echo "FillDWord did not lower through the RTL" >&2
	exit 1
fi
if ! grep -Eq '::u_system::p_fillbyte' \
	"$tmp/filldword_builtin.cc"
then
	echo "FillByte did not lower through the RTL" >&2
	exit 1
fi

tpcc_build "$tmp/filldword_builtin_pascal" \
	"$tmp/filldword_builtin.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/filldword_builtin_pascal"

tpcc_build "$tmp/filldword_builtin_runtime" \
	tests/filldword_builtin_runtime.cpp
tpcc_run \
	"$tmp/filldword_builtin_runtime"

echo "FillByte/FillDWord builtin tests passed"
