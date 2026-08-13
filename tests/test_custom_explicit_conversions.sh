#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/custom_explicit_conversions.cc" \
	tests/custom_explicit_conversions.pp

for required in \
	'o_explicit(' \
	'o_implicit(' \
	'o_unchecked_implicit(' \
	'm_conversion_target<t_ttarget>' \
	'm_conversion_target<t_tothertarget>' \
	'p_explicit('
do
	if ! grep -Fq "$required" \
		"$tmp/custom_explicit_conversions.cc"
	then
		echo "missing explicit-conversion lowering: $required" >&2
		exit 1
	fi
done

tpcc_build "$tmp/custom_explicit_conversions" \
	"$tmp/custom_explicit_conversions.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/custom_explicit_conversions"

if tpcc_translate -dREJECT_EXPLICIT_AS_IMPLICIT \
	-o"$tmp/explicit_as_implicit.cc" \
	tests/custom_explicit_conversions.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an Explicit-only conversion implicitly" >&2
	exit 1
fi
if ! grep -Fq 'no implicit conversion' "$tmp/stderr"
then
	echo "wrong Explicit-only implicit-conversion diagnostic" >&2
	sed -n '1,120p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/custom_explicit_as_rejected.cc" \
	tests/custom_explicit_as_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted operator Explicit through 'as'" >&2
	exit 1
fi
if ! grep -Fq \
	"'as' requires compatible real-number or class/interface types" \
	"$tmp/stderr"
then
	echo "wrong custom 'as' diagnostic" >&2
	sed -n '1,120p' "$tmp/stderr" >&2
	exit 1
fi

echo "custom explicit-conversion tests passed"
