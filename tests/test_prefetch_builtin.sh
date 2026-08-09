#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-prefetch-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/prefetch_builtin.cc" \
	tests/prefetch_builtin.pp

if ! rg -q \
	'::u_system::p_prefetch' \
	"$tmp/prefetch_builtin.cc"
then
	echo "Prefetch did not lower through the RTL" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-O2 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/prefetch_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/prefetch_builtin"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/prefetch_builtin"

echo "Prefetch builtin tests passed"
