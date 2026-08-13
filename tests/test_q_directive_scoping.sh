#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/q_directive_scoping.cc" \
	tests/q_directive_scoping.pp

grep -Eq \
	'for \(p_loopunchecked = .*::u_system::m_unchecked_succ\(p_loopunchecked\)' \
	"$tmp/q_directive_scoping.cc"
grep -Eq \
	'for \(p_loopchecked = .*::u_system::p_succ\(p_loopchecked\)' \
	"$tmp/q_directive_scoping.cc"
grep -Eq \
	'for \(p_downunchecked = .*::u_system::m_unchecked_pred\(p_downunchecked\)' \
	"$tmp/q_directive_scoping.cc"
grep -Eq \
	'for \(p_downchecked = .*::u_system::p_pred\(p_downchecked\)' \
	"$tmp/q_directive_scoping.cc"

tpcc_build "$tmp/q_directive_scoping" \
	"$tmp/q_directive_scoping.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/q_directive_scoping"

tpcc_translate \
	-o"$tmp/q_directive_scoping_constants.cc" \
	tests/q_directive_scoping_constants.pp
tpcc_build "$tmp/q_directive_scoping_constants" \
	"$tmp/q_directive_scoping_constants.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/q_directive_scoping_constants"

for define in TEST_ABS TEST_SUCC TEST_PRED
do
	if tpcc_translate \
		-d"$define" \
		-o"$tmp/checked_constant.cc" \
		tests/q_directive_scoping_checked_constant.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "$define checked outer call unexpectedly compiled" >&2
		exit 1
	fi
	grep -Fq 'integer constant overflow' \
		"$tmp/stderr"
done

echo "Q directive-scoping tests passed"
