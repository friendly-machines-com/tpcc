#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-findfirst-findnext-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/work/files/subdir"

printf abc >"$tmp/work/files/alpha1.dat"
printf 12345 >"$tmp/work/files/alpha2.dat"
printf secret >"$tmp/work/files/.secret"
printf readonly >"$tmp/work/files/readonly.dat"
chmod 444 "$tmp/work/files/readonly.dat"
mkfifo "$tmp/work/files/pipe"
ln -s alpha1.dat "$tmp/work/files/file-link"
ln -s subdir "$tmp/work/files/dir-link"

cd "$root"

./mp -Furtl -o"$tmp/findfirst_findnext.cc" \
	tests/findfirst_findnext.pp
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
	"$tmp/findfirst_findnext.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/findfirst_findnext"

cd "$tmp/work"
ASAN_OPTIONS=detect_leaks=1 "$tmp/findfirst_findnext"

echo "FindFirst/FindNext tests passed"
