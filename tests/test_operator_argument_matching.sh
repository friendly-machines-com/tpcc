#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-operator-argument-matching.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/operator_argument_matching.cc" \
	tests/operator_argument_matching.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/operator_argument_matching.cc" \
	"$tmp/system.cc" \
	-o "$tmp/operator_argument_matching"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/operator_argument_matching"

if ./mp -Furtl \
	-o"$tmp/symbol_identifier_rejected.cc" \
	tests/symbol_identifier_rejected.pp \
	>"$tmp/symbol_identifier_rejected.out" 2>&1
then
	echo "accepted an operator symbol as a procedure identifier" >&2
	exit 1
fi

if ! rg -Fq 'error: expected identifier' \
	"$tmp/symbol_identifier_rejected.out"
then
	echo "wrong symbolic-identifier diagnostic" >&2
	cat "$tmp/symbol_identifier_rejected.out" >&2
	exit 1
fi

echo "operator argument matching tests passed"
