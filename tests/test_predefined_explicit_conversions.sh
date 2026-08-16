#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/predefined_explicit_conversions.cc" \
	tests/predefined_explicit_conversions.pp

for required in \
	'reinterpret_cast<t_tbase*>' \
	'm_ordinal_cast<::u_system::t_ptruint>' \
	'::u_system::m_set_cast<'
do
	if ! grep -Fq "$required" \
		"$tmp/predefined_explicit_conversions.cc"
	then
		echo "missing predefined explicit-conversion lowering: $required" >&2
		exit 1
	fi
done

tpcc_build "$tmp/predefined_explicit_conversions" \
	"$tmp/predefined_explicit_conversions.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/predefined_explicit_conversions"

for rejection in \
	RECORD \
	ARRAY \
	REAL_ORDINAL \
	UNRELATED_CLASS \
	MANAGED_PACKED \
	SET_PACKED \
	CHAIN \
	OPERATOR_SOURCE_WIDENING \
	NONBYTE_SCALAR_VIEW \
	WRONG_SIZE_BYTE_VIEW \
	AGGREGATE_BYTE_VIEW
do
	if tpcc_translate -dREJECT_"$rejection" \
		-o"$tmp/rejected.cc" \
		tests/predefined_explicit_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid explicit conversion: $rejection" >&2
		exit 1
	fi
	for required in \
		'invalid explicit conversion' \
		'expected type' \
		'but got type'
	do
		if ! grep -Fq "$required" "$tmp/stderr"
		then
			echo "incomplete explicit-conversion diagnostic for $rejection" >&2
			sed -n '1,160p' "$tmp/stderr" >&2
			exit 1
		fi
	done
done

echo "predefined explicit-conversion tests passed"
