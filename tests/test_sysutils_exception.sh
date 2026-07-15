#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-sysutils-exception-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/sysutils_exception.cc" tests/sysutils_exception.pp
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
	"$tmp/sysutils_exception.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/sysutils_exception"
ASAN_OPTIONS=detect_leaks=1 "$tmp/sysutils_exception"

echo "SysUtils Exception tests passed"
