#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-tdatetime-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/tdatetime.cc" \
	tests/tdatetime.pp

if ! rg -Fq \
	'using t_tdatetime = ::u_system::t_double;' \
	"$tmp/system.h"
then
	echo "TDateTime does not use the Double carrier" >&2
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
	"$tmp/tdatetime.cc" \
	"$tmp/system.cc" \
	-o "$tmp/tdatetime"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/tdatetime"

echo "TDateTime tests passed"
