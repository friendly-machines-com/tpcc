#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-custom-named-unary-operators.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/custom_named_unary_operators.cc" \
	tests/custom_named_unary_operators.pp

rg -Fq 'o_trunc' \
	"$tmp/custom_named_unary_operators.cc"
rg -Fq 'o_round' \
	"$tmp/custom_named_unary_operators.cc"

${CXX:-g++} \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/custom_named_unary_operators.cc" \
	"$tmp/system.cc" \
	-o "$tmp/custom_named_unary_operators"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/custom_named_unary_operators"

echo "custom named-unary operator tests passed"
