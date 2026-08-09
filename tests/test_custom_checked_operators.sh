#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-custom-checked-operators.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/custom_checked_operators.cc" \
	tests/custom_checked_operators.pp

rg -Fq 'o_unchecked_add' \
	"$tmp/custom_checked_operators.cc"
rg -Fq 'o_add' \
	"$tmp/custom_checked_operators.cc"
rg -Fq 'o_operator_plus' \
	"$tmp/custom_checked_operators.cc"
rg -Fq 'p_add' \
	"$tmp/custom_checked_operators.cc"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/custom_checked_operators.cc" \
	"$tmp/system.cc" \
	-o "$tmp/custom_checked_operators"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/custom_checked_operators"

echo "custom checked-operator tests passed"
