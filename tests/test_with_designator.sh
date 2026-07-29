#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-with-designator-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/with_designator.cc" \
	tests/with_designator.pp

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
	"$tmp/with_designator.cc" \
	"$tmp/system.cc" \
	-o "$tmp/with_designator"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/with_designator"

echo "with-designator tests passed"
