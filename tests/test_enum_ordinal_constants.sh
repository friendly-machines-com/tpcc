#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

tpcc_translate \
	-o"$tmp/enum_ordinal_constants.cc" \
	tests/enum_ordinal_constants.pp

tpcc_build \
	"$tmp/enum_ordinal_constants" \
	"$tmp/enum_ordinal_constants.cc" \
	"$tmp/system.cc"

tpcc_run "$tmp/enum_ordinal_constants"

for source in \
	tests/cross_enum_named_value.pp \
	tests/cross_enum_unnamed_value.pp
do
	if tpcc_translate \
		-o"$tmp/rejected.cc" \
		"$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted a foreign enum as an explicit enum value: $source" >&2
		exit 1
	fi
	if ! grep -Fq 'explicit enum member value' "$tmp/stderr"; then
		echo "wrong foreign-enum diagnostic for $source" >&2
		sed -n '1,80p' "$tmp/stderr" >&2
		exit 1
	fi
done

for source in \
	tests/enum_shortstring_capacity_named.pp \
	tests/enum_shortstring_capacity_unnamed.pp
do
	if tpcc_translate \
		-o"$tmp/rejected.cc" \
		"$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted an enum-valued ShortString capacity: $source" >&2
		exit 1
	fi
	if ! grep -Fq \
		'shortstring capacity must have an integer type' \
		"$tmp/stderr"
	then
		echo "wrong ShortString-capacity diagnostic for $source" >&2
		sed -n '1,80p' "$tmp/stderr" >&2
		exit 1
	fi
done

if tpcc_translate \
	-o"$tmp/rejected.cc" \
	tests/mismatched_enum_subrange.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted bounds from different enum types" >&2
	exit 1
fi
if ! grep -Fq \
	'subrange constructor with bounds from the same enum type' \
	"$tmp/stderr"
then
	echo "wrong mismatched-enum subrange diagnostic" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/rejected.cc" \
	tests/integer_enum_subrange.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted mixed integer and enum subrange bounds" >&2
	exit 1
fi
if ! grep -Fq \
	'subrange bounds must be compatible ordinal constants' \
	"$tmp/stderr"
then
	echo "wrong mixed-domain subrange diagnostic" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

echo "enum ordinal constant tests passed"
