#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-rmdir-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p \
	"$tmp/work/short-empty" \
	"$tmp/work/ansi-empty" \
	"$tmp/work/pending-empty" \
	"$tmp/work/nonempty/child"
printf data >"$tmp/work/regular-file"

cd "$root"

./mp -Furtl -o"$tmp/rmdir.cc" tests/rmdir.pp
if ! rg -Fq '::u_system::p_rmdir' "$tmp/rmdir.cc"
then
	echo "checked RmDir did not use its System RTL operation" >&2
	exit 1
fi
if ! rg -Fq '::u_system::m_unchecked_rmdir' "$tmp/rmdir.cc"
then
	echo "unchecked RmDir did not use its System RTL operation" >&2
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
	"$tmp/rmdir.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/rmdir"

cd "$tmp/work"
ASAN_OPTIONS=detect_leaks=1 "$tmp/rmdir"

echo "RmDir tests passed"
