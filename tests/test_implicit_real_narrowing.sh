#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-implicit-real-narrowing.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/implicit_real_narrowing.cc" \
	tests/implicit_real_narrowing.pp

rg -Fq '::u_system::m_real_cast' \
	"$tmp/implicit_real_narrowing.cc"
rg -Fq '::u_system::m_range_checked_real_cast' \
	"$tmp/implicit_real_narrowing.cc"

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
	"$tmp/implicit_real_narrowing.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/implicit_real_narrowing"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/implicit_real_narrowing"

if ./mp -Furtl -dCHECK_CONSTANT_REJECTION \
	-o"$tmp/constant_rejected.cc" \
	tests/implicit_real_narrowing.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an out-of-range checked real constant" >&2
	exit 1
fi

if ! rg -Fq \
	'real constant out of range for target type' \
	"$tmp/stderr"
then
	echo "wrong checked real constant diagnostic" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

echo "implicit real narrowing tests passed"
