#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/explicit_enum_values.cc" \
	tests/explicit_enum_values.pp

for expected in \
	'p_middle = 5' \
	'p_lowest = -2' \
	'p_highest = 9' \
	'p_back = 1' \
	'p_indextwo = 2, p_indexthree, p_indexfour' \
	'p_calculated = 16' \
	'p_charactera = 65'
do
	if ! grep -Fq "$expected" "$tmp/explicit_enum_values.cc"; then
		echo "missing explicit enum lowering: $expected" >&2
		exit 1
	fi
done

tpcc_build "$tmp/explicit_enum_values" \
	"$tmp/explicit_enum_values.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/explicit_enum_values"

if tpcc_translate -o"$tmp/rejected.cc" \
	tests/explicit_enum_value_out_of_range.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an explicit enum value outside signed 32-bit range" >&2
	exit 1
fi
if ! grep -Fq \
	'explicit enum value is outside signed 32-bit range' \
	"$tmp/stderr"
then
	echo "wrong diagnostic for out-of-range explicit enum value" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

for source in \
	tests/duplicate_enum_ordinal.pp \
	tests/duplicate_enum_implicit_collision.pp
do
	if tpcc_translate -o"$tmp/rejected-duplicate.cc" \
		"$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted duplicate enum ordinal in $source" >&2
		exit 1
	fi
	if ! grep -Fq 'duplicate enum ordinal' "$tmp/stderr"; then
		echo "wrong diagnostic for duplicate enum ordinal in $source" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "explicit enum value tests passed"
