#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-set-literals-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/set_literals.cc" tests/set_literals.pp
if ! rg -q '::u_system::t_set<::u_system::t_char>' "$tmp/set_literals.cc"; then
	echo "Char set lost its item type" >&2
	exit 1
fi
if ! rg -q '::u_system::tpcc_set_range' "$tmp/set_literals.cc"; then
	echo "set range did not lower through the RTL" >&2
	exit 1
fi
if ! rg -q '::u_system::o_in' "$tmp/set_literals.cc"; then
	echo "set membership did not lower through the RTL" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	tests/set_literals_runtime.cc \
	"$tmp/system.cc" \
	-o "$tmp/set_literals"
ASAN_OPTIONS=detect_leaks=1 "$tmp/set_literals"

echo "set literal tests passed"
