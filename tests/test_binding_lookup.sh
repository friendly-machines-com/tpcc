#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-binding-lookup-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/binding_lookup.cc" \
	tests/binding_lookup.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/binding_lookup.cc" \
	"$tmp/system.cc" \
	-o "$tmp/binding_lookup"
ASAN_OPTIONS=detect_leaks=1 "$tmp/binding_lookup"

if ./mp -Furtl -o"$tmp/rejected.cc" \
	tests/binding_same_scope_rejected.pp \
	>"$tmp/rejected.out" 2>&1
then
	echo "same-scope type/value duplicate was accepted" >&2
	exit 1
fi
if ! rg -Fq "duplicate identifier: x" \
	"$tmp/rejected.out"
then
	echo "same-scope duplicate produced the wrong diagnostic" >&2
	cat "$tmp/rejected.out" >&2
	exit 1
fi

echo "binding-lookup tests passed"
