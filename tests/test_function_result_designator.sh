#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-function-result-designator-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/function_result_designator.cc" \
	tests/function_result_designator.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/function_result_designator.cc" \
	"$tmp/system.cc" \
	-o "$tmp/function_result_designator"

ASAN_OPTIONS=detect_leaks=1 "$tmp/function_result_designator"

echo "function-result designator test passed"
