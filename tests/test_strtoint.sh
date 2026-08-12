#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-strtoint-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/strtoint.cc" tests/strtoint.pp

if ! rg -Fq '::u_sysutils::p_strtoint(' "$tmp/strtoint.cc"; then
	echo "SysUtils.StrToInt did not resolve in the SysUtils namespace" >&2
	exit 1
fi
if ! rg -Fq '::u_system::p_val(' "$tmp/sysutils.cc"; then
	echo "SysUtils.StrToInt did not delegate parsing to System.Val" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/strtoint.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/strtoint"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/strtoint"

echo "StrToInt tests passed"
