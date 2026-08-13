#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/pos_builtin.cc" tests/pos_builtin.pp
if ! grep -F -q 'tpcc_shortstring_from_c<255>("\141\000\142", 3)' "$tmp/pos_builtin.cc"; then
	echo "embedded-NUL Pascal literal lost its explicit byte length" >&2
	exit 1
fi
tpcc_build "$tmp/pos_builtin" \
	"$tmp/pos_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/pos_builtin"

tpcc_build "$tmp/pos_char_runtime" \
	tests/pos_char_runtime.cpp
tpcc_run "$tmp/pos_char_runtime"

echo "Pos builtin tests passed"
