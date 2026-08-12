#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-decode-date-time-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/decode_date_time.cc" \
	tests/decode_date_time.pp

if ! rg -Fq '::u_sysutils::p_decodedate' \
	"$tmp/decode_date_time.cc"
then
	echo "SysUtils.DecodeDate did not use p_decodedate" >&2
	exit 1
fi

if ! rg -Fq '::u_sysutils::p_decodetime' \
	"$tmp/decode_date_time.cc"
then
	echo "SysUtils.DecodeTime did not use p_decodetime" >&2
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
	"$tmp/decode_date_time.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/decode_date_time"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/decode_date_time"

echo "DecodeDate and DecodeTime tests passed"
