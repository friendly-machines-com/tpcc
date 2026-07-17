#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-index-range-checking.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/index_range_checking.cc" \
	tests/index_range_checking.pp

rg -Fq '::u_system::m_unchecked_index' \
	"$tmp/index_range_checking.cc"
rg -Fq '::u_system::p_index' \
	"$tmp/index_range_checking.cc"
rg -Fq '::u_system::m_ordinal_cast' \
	"$tmp/index_range_checking.cc"
rg -Fq '::u_system::m_range_checked_ordinal_cast' \
	"$tmp/index_range_checking.cc"

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
	"$tmp/index_range_checking.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/index_range_checking"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/index_range_checking"

echo "index range-checking tests passed"
