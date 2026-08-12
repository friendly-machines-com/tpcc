#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-swapendian-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/swapendian_builtin.cc" \
	tests/swapendian_builtin.pp

if ! rg -Fq \
	'::u_system::p_swapendian(' \
	"$tmp/swapendian_builtin.cc"
then
	echo "SwapEndian calls did not use the System RTL entry point" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/swapendian_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/swapendian_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/swapendian_builtin"

echo "SwapEndian builtin tests passed"
