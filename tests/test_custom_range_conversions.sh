#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-custom-range-conversions.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/custom_range_conversions.cc" \
	tests/custom_range_conversions.pp

rg -Fq 'o_implicit' \
	"$tmp/custom_range_conversions.cc"
rg -Fq 'o_unchecked_implicit' \
	"$tmp/custom_range_conversions.cc"
rg -Fq 'p_implicit' \
	"$tmp/custom_range_conversions.cc"
rg -Fq 'p_uncheckedimplicit' \
	"$tmp/custom_range_conversions.cc"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/custom_range_conversions.cc" \
	"$tmp/system.cc" \
	-o "$tmp/custom_range_conversions"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/custom_range_conversions"

for omitted in CHECKED UNCHECKED
do
	if ./mp -Furtl -d"OMIT_${omitted}" \
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
done

for mode in normal reverse
do
	define=
	if test "$mode" = reverse
	then
		define=-dREVERSE
	fi
	if ./mp -Furtl $define \
		-o"$tmp/contract_${mode}.cc" \
		tests/implicit_conversion_contract_mismatch_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "attached a conversion body to a different operator contract: $mode" >&2
		exit 1
	fi
	if ! rg -Fq 'duplicate identifier or overload directive mismatch' \
		"$tmp/stderr"
	then
		echo "wrong diagnostic for conversion contract mismatch: $mode" >&2
		sed -n '1,100p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "custom range-conversion tests passed"
