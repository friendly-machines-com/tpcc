#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/setstring_builtin.cc" \
	tests/setstring_builtin.pp

if ! grep -Fq \
	'::u_system::p_setstring(' \
	"$tmp/setstring_builtin.cc"
then
	echo "SetString call did not use the System RTL entry point" >&2
	exit 1
fi

tpcc_build "$tmp/setstring_builtin" \
	"$tmp/setstring_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/setstring_builtin"

if tpcc_translate \
	-o"$tmp/setstring_non_string_rejected.cc" \
	tests/setstring_non_string_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "SetString accepted a non-string out destination" >&2
	exit 1
fi
if ! grep -Fq \
	"no matching overload for 'setstring'" \
	"$tmp/stderr"
then
	echo "SetString produced the wrong non-string diagnostic" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

echo "SetString builtin tests passed"
