#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-implicit-conversion-single-edge.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/single_edge.cc" \
	tests/implicit_conversion_single_edge.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/single_edge.cc" \
	"$tmp/system.cc" \
	-o "$tmp/single_edge"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/single_edge"

for kind in RECORD INTEGER NARROW SUBRANGE
do
	if ./mp -Furtl -d"OMIT_${kind}_DIRECT" \
		-o"$tmp/chained.cc" \
		tests/implicit_conversion_single_edge.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted an implicit A -> B -> C conversion chain: $kind" >&2
		exit 1
	fi

	if ! rg -Fq 'no implicit conversion' "$tmp/stderr"
	then
		echo "wrong diagnostic for rejected implicit conversion chain: $kind" >&2
		sed -n '1,80p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "single-edge implicit conversion tests passed"
