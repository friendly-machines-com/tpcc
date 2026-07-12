#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-write-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/write_builtin.cc" tests/write_builtin.pp
if ! rg -Fq 'pas::p_write(' "$tmp/write_builtin.cc"; then
	echo "Write did not lower through the RTL" >&2
	exit 1
fi
if ! rg -Fq 'pas::p_writeln(' "$tmp/write_builtin.cc"; then
	echo "WriteLn did not lower through the RTL" >&2
	exit 1
fi
if ! rg -Fq 'pas::tpcc_make_write_arg(' "$tmp/write_builtin.cc"; then
	echo "formatted output arguments lost their RTL descriptors" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	tests/write_builtin_runtime.cc \
	rtl/system.cc \
	-o "$tmp/write_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/write_builtin"

echo "Write/WriteLn builtin tests passed"
