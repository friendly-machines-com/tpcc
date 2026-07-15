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
	'const t_touter p_outer = [](' \
	'::u_system::tpcc_make_set<t_tflag>' \
	'p_packedvalues = {{[](' \
	'tpcc_record.m_set_p_code' \
	'tpcc_record.m_set_p_value'
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
	rtl/system.cc \
	-o "$tmp/typed_const_aggregates"
ASAN_OPTIONS=detect_leaks=1 "$tmp/typed_const_aggregates"

echo "typed aggregate constant tests passed"
