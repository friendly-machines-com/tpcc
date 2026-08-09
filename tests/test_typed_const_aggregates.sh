#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-typed-const-aggregates-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/typed_const_aggregates.cc" \
	tests/typed_const_aggregates.pp

for expected in \
	't_touter p_outer = [](' \
	'::u_system::tpcc_make_set<t_tflag>' \
	'p_packedvalues = {{[](' \
	'tpcc_record.m_set_p_code' \
	'tpcc_record.m_set_p_value' \
	'::u_system::tpcc_ansistring_literal(' \
	'::u_system::o_implicit(' \
	'::u_system::tpcc_shortstring_cast<3>'
do
	if ! rg -Fq "$expected" "$tmp/typed_const_aggregates.cc"; then
		echo "missing typed-constant lowering: $expected" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/typed_const_aggregates.cc" \
	"$tmp/system.cc" \
	-o "$tmp/typed_const_aggregates"
ASAN_OPTIONS=detect_leaks=1 "$tmp/typed_const_aggregates"

if ./mp -Furtl \
	-o"$tmp/custom_conversion_constant_rejected.cc" \
	tests/custom_conversion_constant_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "user-written conversion body was executed during constant evaluation" >&2
	exit 1
fi
expected=$(sed -n '1p' tests/custom_conversion_constant_rejected.error)
if ! rg -Fq "$expected" "$tmp/stderr"
then
	echo "wrong custom-conversion constant diagnostic; expected: $expected" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

echo "typed aggregate constant tests passed"
