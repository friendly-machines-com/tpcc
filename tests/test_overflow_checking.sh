#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-overflow-checking.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/overflow_checking.cc" \
	tests/overflow_checking.pp

rg -Fq '::u_system::o_unchecked_add' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_add' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_unchecked_negative' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_negative' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_unchecked_intdivide' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_intdivide' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_unchecked_subtract' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_subtract' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_unchecked_multiply' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_multiply' \
	"$tmp/overflow_checking.cc"

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
	"$tmp/overflow_checking.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/overflow_checking"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/overflow_checking"

echo "overflow-checking tests passed"
