#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-strpas-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/strpas.cc" tests/strpas.pp

if ! rg -Fq '::u_sysutils::p_strpas(' "$tmp/strpas.cc"; then
	echo "SysUtils.StrPas did not resolve in the SysUtils namespace" >&2
	exit 1
fi
if ! rg -Fq '::u_system::o_implicit(' "$tmp/sysutils.cc"; then
	echo "SysUtils.StrPas did not use the existing PChar-to-AnsiString conversion" >&2
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
	"$tmp/strpas.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/strpas"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/strpas"

echo "StrPas tests passed"
