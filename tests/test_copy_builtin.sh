#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-copy-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/copy_builtin.cc" tests/copy_builtin.pp
if ! rg -q '::u_system::p_copy\(' "$tmp/copy_builtin.cc"; then
	echo "Copy did not lower to its ordinary RTL call" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	tests/copy_builtin_runtime.cc \
	"$tmp/system.cc" \
	-o "$tmp/copy_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/copy_builtin"

echo "Copy builtin tests passed"
