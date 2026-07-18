#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-io-checking-test.$$
trap 'rm -rf "$tmp"; rm -f /tmp/tpcc-i-check-runtime.tmp /tmp/tpcc-i-check-other.tmp' \
	EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/io_checking.cc" \
	tests/io_checking.pp

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
	"$tmp/io_checking.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/io_checking"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/io_checking"

echo "I/O checking tests passed"
