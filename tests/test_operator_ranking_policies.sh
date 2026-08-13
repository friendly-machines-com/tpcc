#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/operator_ranking_policies.cc" \
	tests/operator_ranking_policies.pp

tpcc_build "$tmp/operator_ranking_policies" \
	"$tmp/operator_ranking_policies.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/operator_ranking_policies"

for policy in \
	ENUM_ARITHMETIC \
	ENUM_BITWISE \
	ENUM_SHIFT \
	CHAR_ARITHMETIC \
	ARRAY_CONCATENATION
do
	if tpcc_translate -d"$policy" \
		-o"$tmp/rejected.cc" \
		tests/operator_ranking_policy_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted forbidden operator policy: $policy" >&2
		exit 1
	fi
	if ! grep -Fq 'no matching overload' "$tmp/stderr"
	then
		echo "wrong forbidden-operator diagnostic: $policy" >&2
		sed -n '1,140p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "operator ranking policy tests passed"
