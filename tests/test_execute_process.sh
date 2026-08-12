#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-execute-process-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	tests/execute_process_child.cpp \
	-o "$tmp/child with space"

./mp -Furtl -o"$tmp/execute_process.cc" \
	tests/execute_process.pp
for required in \
	'::u_sysutils::p_executeprocess_commandline' \
	'::u_sysutils::p_executeprocess_arguments'
do
	if ! rg -Fq "$required" "$tmp/sysutils.cc"
	then
		echo "ExecuteProcess did not use $required" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/execute_process.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/execute_process"

TPCC_EXECUTE_PROCESS_CHILD="$tmp/child with space" \
TPCC_EXECUTE_PROCESS_INHERITED=present \
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/execute_process"

echo "ExecuteProcess tests passed"
