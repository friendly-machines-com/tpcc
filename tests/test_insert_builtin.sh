#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/insert_builtin.cc" tests/insert_builtin.pp
tpcc_build "$tmp/insert_builtin" \
	"$tmp/insert_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/insert_builtin"

tpcc_build "$tmp/insert_runtime" \
	tests/insert_runtime.cpp
tpcc_run "$tmp/insert_runtime"

if tpcc_translate -o"$tmp/rejected.cc" \
	tests/insert_non_string_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "Insert accepted a non-ShortString generic destination" >&2
	exit 1
fi
if ! grep -Fq "no matching overload for 'insert'" "$tmp/stderr"; then
	echo "Insert produced the wrong non-ShortString diagnostic" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "Insert builtin tests passed"
