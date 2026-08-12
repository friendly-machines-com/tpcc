#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-inttostr-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/inttostr.cc" tests/inttostr.pp

if ! rg -Fq '::u_sysutils::p_inttostr(' "$tmp/inttostr.cc"; then
	echo "SysUtils.IntToStr did not resolve in the SysUtils namespace" >&2
	exit 1
fi
if [ "$(rg -Fc '::u_system::p_str(' "$tmp/sysutils.cc")" -lt 3 ]; then
	echo "SysUtils.IntToStr overloads did not delegate to System.Str" >&2
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
	"$tmp/inttostr.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/inttostr"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/inttostr"

echo "IntToStr tests passed"
