#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-file-lifecycle-test.$$
trap 'rm -rf "$tmp"; rm -f /tmp/tpcc-file-lifecycle-binary /tmp/tpcc-file-lifecycle-text' \
	EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	tests/file_lifecycle_runtime.cc \
	-o "$tmp/file_lifecycle"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/file_lifecycle"

echo "file lifecycle tests passed"
