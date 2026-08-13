#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/custom_in_operator.cc" \
	tests/custom_in_operator.pp

if ! grep -Fq 'o_in(' "$tmp/custom_in_operator.cc"
then
	echo "custom In did not lower through its ordinary operator declaration" >&2
	exit 1
fi
if ! grep -Fq '::u_system::o_in(' "$tmp/custom_in_operator.cc"
then
	echo "System set membership fallback was not retained" >&2
	exit 1
fi

tpcc_build "$tmp/custom_in_operator" \
	"$tmp/custom_in_operator.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/custom_in_operator"

for rejection in ITEM CONTAINER
do
	if tpcc_translate -dREJECT_"$rejection" \
		-o"$tmp/rejected.cc" \
		tests/custom_in_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid In operands: $rejection" >&2
		exit 1
	fi
	for required in \
		"no matching overload for 'in'" \
		'arg 1:' \
		'arg 2:' \
		'all candidates:'
	do
		if ! grep -Fq "$required" "$tmp/stderr"
		then
			echo "incomplete In diagnostic for $rejection" >&2
			sed -n '1,180p' "$tmp/stderr" >&2
			exit 1
		fi
	done
done

if tpcc_translate \
	-o"$tmp/ambiguous.cc" \
	tests/custom_in_ambiguous.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted ambiguous typed custom In overloads" >&2
	exit 1
fi
for required in \
	"ambiguous overload for 'in'" \
	'viable ranks [generic, generic]' \
	'viable ranks [convert, exact]' \
	'tchoicea' \
	'tchoiceb'
do
	if ! grep -Fiq "$required" "$tmp/stderr"
	then
		echo "incomplete ambiguous In diagnostic" >&2
		sed -n '1,220p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "custom In operator tests passed"
