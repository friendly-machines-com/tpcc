#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/set_arithmetic.cc" \
	tests/set_arithmetic.pp

for operation in \
	'::u_system::o_unchecked_add(' \
	'::u_system::o_unchecked_subtract(' \
	'::u_system::o_add(' \
	'::u_system::o_subtract(' \
	'::u_system::o_multiply(' \
	'::u_system::o_symmetric_difference(' \
	'::u_system::o_equal(' \
	'::u_system::o_lessthanorequal(' \
	'::u_system::o_greaterthanorequal('
do
	if ! rg -Fq "$operation" \
		"$tmp/set_arithmetic.cc"
	then
		echo "missing predefined set operation: $operation" >&2
		exit 1
	fi
done

tpcc_build "$tmp/set_arithmetic" \
	"$tmp/set_arithmetic.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/set_arithmetic"

if tpcc_translate -o"$tmp/rejected.cc" \
	tests/set_arithmetic_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted set arithmetic with incompatible item domains" >&2
	exit 1
fi
for required in \
	"no matching overload for 'uncheckedadd'" \
	'arg 1:' \
	'arg 2:' \
	'all candidates:'
do
	if ! rg -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete incompatible-set diagnostic" >&2
		sed -n '1,180p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "set arithmetic tests passed"
