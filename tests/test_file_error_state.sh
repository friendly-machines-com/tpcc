#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-file-error-state-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
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
	tests/file_error_state_runtime.cpp \
	-o "$tmp/file_error_state"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/file_error_state"

echo "file error-state tests passed"
