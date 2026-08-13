#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/q_intrinsics.cc" \
	tests/q_intrinsics.pp

for operation in \
	p_abs m_unchecked_abs \
	p_succ m_unchecked_succ \
	p_pred m_unchecked_pred
do
	grep -Fq "::u_system::$operation" \
		"$tmp/q_intrinsics.cc"
done

tpcc_build "$tmp/q_intrinsics" \
	"$tmp/q_intrinsics.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/q_intrinsics"

tpcc_translate \
	-o"$tmp/unchecked_constants.cc" \
	tests/q_intrinsics_unchecked_constants.pp
tpcc_build "$tmp/unchecked_constants" \
	"$tmp/unchecked_constants.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/unchecked_constants"

for define in TEST_ABS TEST_SUCC TEST_PRED
do
	if tpcc_translate \
		-d"$define" \
		-o"$tmp/checked_constant.cc" \
		tests/q_intrinsics_checked_constant.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "$define checked constant unexpectedly compiled" >&2
		exit 1
	fi
	grep -Fq 'integer constant overflow' \
		"$tmp/stderr"
done

echo "Q intrinsic tests passed"
