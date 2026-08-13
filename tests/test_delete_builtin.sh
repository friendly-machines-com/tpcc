#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/delete_builtin.cc" tests/delete_builtin.pp
tpcc_build "$tmp/delete_builtin" \
	"$tmp/delete_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/delete_builtin"

if tpcc_translate -o"$tmp/rejected.cc" \
	tests/delete_non_string_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "Delete accepted a non-ShortString generic destination" >&2
	exit 1
fi
if ! grep -Fq "no matching overload for 'delete'" "$tmp/stderr"; then
	echo "Delete produced the wrong non-ShortString diagnostic" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "Delete builtin tests passed"
