#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-filldword-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/filldword_builtin.cc" \
	tests/filldword_builtin.pp

if ! rg -q '::u_system::p_filldword' \
	"$tmp/filldword_builtin.cc"
then
	echo "FillDWord did not lower through the RTL" >&2
	exit 1
fi

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
	"$tmp/filldword_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/filldword_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/filldword_builtin_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	tests/filldword_builtin_runtime.cpp \
	-o "$tmp/filldword_builtin_runtime"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/filldword_builtin_runtime"

echo "FillDWord builtin tests passed"
