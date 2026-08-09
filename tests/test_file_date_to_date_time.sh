#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-file-date-to-date-time-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/file_date_to_date_time.cc" \
	tests/file_date_to_date_time.pp

if ! rg -Fq '::u_system::p_filedatetodatetime' \
	"$tmp/file_date_to_date_time.cc"
then
	echo "SysUtils.FileDateToDateTime did not use p_filedatetodatetime" >&2
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
	"$tmp/file_date_to_date_time.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/file_date_to_date_time"

TZ=UTC0 \
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/file_date_to_date_time"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	tests/file_date_to_date_time_runtime.cpp \
	-o "$tmp/file_date_to_date_time_runtime"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/file_date_to_date_time_runtime"

echo "FileDateToDateTime tests passed"
