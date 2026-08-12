#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-unix-fpsystem-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/unix_fpsystem.cc" \
	tests/unix_fpsystem.pp

if ! rg -Fq '::u_unix::p_fpsystem' \
	"$tmp/unix_fpsystem.cc"
then
	echo "Unix.FpSystem did not use p_fpsystem" >&2
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
	"$tmp/unix_fpsystem.cc" \
	"$tmp/unix.cc" \
	"$tmp/system.cc" \
	-o "$tmp/unix_fpsystem"

TPCC_FPSYSTEM_INHERITED=inherited \
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/unix_fpsystem"

echo "Unix.FpSystem tests passed"
