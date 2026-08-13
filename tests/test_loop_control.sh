#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/loop_control.cc" tests/loop_control.pp
tpcc_build "$tmp/loop_control" \
	"$tmp/loop_control.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/loop_control"

for source in tests/loop_break_outside.pp tests/loop_continue_outside.pp; do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" >"$tmp/stdout" 2>"$tmp/stderr"; then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! rg -F -q -- "$expected" "$tmp/stderr"; then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "loop-control tests passed"
