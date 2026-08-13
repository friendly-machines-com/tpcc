#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/unchecked.cc" \
	tests/overflow_constant_unchecked.pp
tpcc_build "$tmp/unchecked" \
	"$tmp/unchecked.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/unchecked"

if tpcc_translate -o"$tmp/checked.cc" \
	tests/overflow_constant_checked_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "checked overflowing constant unexpectedly compiled" >&2
	exit 1
fi
rg -Fq 'integer constant out of range for target type' \
	"$tmp/stderr"

echo "overflow constant-folding tests passed"
