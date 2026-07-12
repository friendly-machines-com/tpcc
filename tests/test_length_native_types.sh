#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-length-native-types-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/length_native_types.cc" tests/length_native_types.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/length_native_types.cc" \
	rtl/system.cc \
	-o "$tmp/length_native_types"
ASAN_OPTIONS=detect_leaks=1 "$tmp/length_native_types"

echo "Length native-integer tests passed"
