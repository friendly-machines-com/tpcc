#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-implicit-ordinal-narrowing.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/implicit_ordinal_narrowing.cc" \
	tests/implicit_ordinal_narrowing.pp

rg -Fq '::u_system::m_ordinal_cast' \
	"$tmp/implicit_ordinal_narrowing.cc"
rg -Fq '::u_system::m_range_checked_ordinal_cast' \
	"$tmp/implicit_ordinal_narrowing.cc"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/implicit_ordinal_narrowing.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/implicit_ordinal_narrowing"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/implicit_ordinal_narrowing"

echo "implicit ordinal narrowing tests passed"
