#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

tpcc_translate \
	-o"$tmp/custom_narrowing_conversions.cc" \
	tests/custom_narrowing_conversions.pp

tpcc_build "$tmp/custom_narrowing_conversions" \
	"$tmp/custom_narrowing_conversions.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/custom_narrowing_conversions"

for omitted in CHECKED UNCHECKED
do
	if tpcc_translate -d"OMIT_${omitted}" \
		-o"$tmp/missing_${omitted}.cc" \
		tests/custom_narrowing_conversions.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted a missing ${omitted} narrowing-conversion family" >&2
		exit 1
	fi
	if ! grep -Fq 'no implicit conversion' "$tmp/stderr"
	then
		echo "wrong diagnostic for missing ${omitted} narrowing conversion" >&2
		sed -n '1,120p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "custom narrowing-conversion tests passed"
