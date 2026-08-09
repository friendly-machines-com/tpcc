#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-get-local-time-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/get_local_time.cc" \
	tests/get_local_time.pp

if ! rg -Fq '::u_system::p_getlocaltime' \
	"$tmp/get_local_time.cc"
then
	echo "SysUtils.GetLocalTime did not use p_getlocaltime" >&2
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
	"$tmp/get_local_time.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/get_local_time"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/get_local_time"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	tests/get_local_time_runtime.cpp \
	-o "$tmp/get_local_time_runtime"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/get_local_time_runtime"

echo "GetLocalTime tests passed"
