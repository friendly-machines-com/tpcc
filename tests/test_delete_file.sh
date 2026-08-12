#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-delete-file-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/work/directory"

printf regular >"$tmp/work/regular"
printf target >"$tmp/work/target"
ln -s target "$tmp/work/file-link"
ln -s missing "$tmp/work/broken-link"

cd "$root"

./mp -Furtl -o"$tmp/delete_file.cc" tests/delete_file.pp

if ! rg -Fq '::u_sysutils::p_deletefile(' "$tmp/delete_file.cc"; then
	echo "SysUtils.DeleteFile did not resolve in the SysUtils namespace" >&2
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
	"$tmp/delete_file.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/delete_file"

cd "$tmp/work"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/delete_file"

test ! -e regular
test ! -L file-link
test ! -L broken-link
test -f target
test -d directory

echo "DeleteFile tests passed"
