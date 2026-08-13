#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/try_statements.cc" tests/try_statements.pp
tpcc_build "$tmp/try_statements" \
	"$tmp/try_statements.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/try_statements"

for source in \
	tests/finally_exit.pp \
	tests/finally_break.pp
do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
	    >"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! grep -F -q -- "$expected" "$tmp/stderr"; then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "try-statement tests passed"
