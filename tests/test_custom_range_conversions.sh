#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/custom_range_conversions.cc" \
	tests/custom_range_conversions.pp

rg -Fq 'o_implicit' \
	"$tmp/custom_range_conversions.cc"
rg -Fq 'o_unchecked_implicit' \
	"$tmp/custom_range_conversions.cc"
rg -Fq 'p_implicit' \
	"$tmp/custom_range_conversions.cc"
rg -Fq 'p_uncheckedimplicit' \
	"$tmp/custom_range_conversions.cc"

tpcc_build "$tmp/custom_range_conversions" \
	"$tmp/custom_range_conversions.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/custom_range_conversions"

for omitted in CHECKED UNCHECKED
do
	if tpcc_translate -d"OMIT_${omitted}" \
		-o"$tmp/missing_${omitted}.cc" \
		tests/custom_range_conversions.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted a missing ${omitted} implicit conversion family" >&2
		exit 1
	fi
	if ! rg -Fq 'no implicit conversion' "$tmp/stderr"
	then
		echo "wrong diagnostic for missing ${omitted} implicit conversion" >&2
		sed -n '1,100p' "$tmp/stderr" >&2
		exit 1
	fi
	for required in \
		'expected return type:' \
		'arg 1:' \
		'all candidates:' \
		'where' \
		'type tsource' \
		'type tdestination' \
		'source:'
	do
		if ! rg -Fq "$required" "$tmp/stderr"
		then
			echo "incomplete implicit-conversion diagnostic: $required" >&2
			sed -n '1,140p' "$tmp/stderr" >&2
			exit 1
		fi
	done
done

for mode in normal reverse
do
	define=
	if test "$mode" = reverse
	then
		define=-dREVERSE
	fi
	if tpcc_translate $define \
		-o"$tmp/contract_${mode}.cc" \
		tests/implicit_conversion_contract_mismatch_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "attached a conversion body to a different operator contract: $mode" >&2
		exit 1
	fi
	if ! rg -Fq 'callable declaration conflicts with existing declaration' \
		"$tmp/stderr"
	then
		echo "wrong diagnostic for conversion contract mismatch: $mode" >&2
		sed -n '1,100p' "$tmp/stderr" >&2
		exit 1
	fi
	for required in \
		'incoming declaration:' \
		'conflicting declaration:' \
		'existing overload family:' \
		'where' \
		'type tsource' \
		'type tdestination' \
		'source:'
	do
		if ! rg -Fq "$required" "$tmp/stderr"
		then
			echo "incomplete conversion-declaration diagnostic: $required" >&2
			sed -n '1,140p' "$tmp/stderr" >&2
			exit 1
		fi
	done
done

echo "custom range-conversion tests passed"
