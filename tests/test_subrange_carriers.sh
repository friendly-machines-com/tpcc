#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-subrange-carriers.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp \
	-Furtl \
	-Futests/subrange_carrier_units \
	-o"$tmp/subrange_carriers.cc" \
	tests/subrange_carrier_units/subrange_carriers.pp

for required in \
	'struct m_subrange_' \
	'using t_tfirst [[maybe_unused]] = ::u_rangecarrier::m_subrange_' \
	'using t_tsecond [[maybe_unused]] = ::u_rangecarrier::m_subrange_' \
	'using t_tfirstalias [[maybe_unused]] = ::u_rangecarrier::m_subrange_' \
	'std::is_standard_layout_v<m_subrange_' \
	'std::is_trivially_copyable_v<m_subrange_'
do
	if ! rg -Fq "$required" "$tmp/rangecarrier.h"
	then
		echo "missing public subrange carrier output: $required" >&2
		exit 1
	fi
done

first_signature=$(rg -F 'p_identify(' "$tmp/rangecarrier.h" | sed -n '1p')
second_signature=$(rg -F 'p_identify(' "$tmp/rangecarrier.h" | sed -n '2p')
if test -z "$first_signature" || test -z "$second_signature" ||
	test "$first_signature" = "$second_signature"
then
	echo "distinct Pascal subranges did not produce distinct C++ overloads" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/subrange_carriers.cc" \
	"$tmp/rangecarrier.cc" \
	"$tmp/system.cc" \
	-o "$tmp/subrange_carriers"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/subrange_carriers"

echo "subrange carrier tests passed"
