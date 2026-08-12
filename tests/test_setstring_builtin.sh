#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-setstring-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/setstring_builtin.cc" \
	tests/setstring_builtin.pp

if ! rg -Fq \
	'::u_system::p_setstring(' \
	"$tmp/setstring_builtin.cc"
then
	echo "SetString call did not use the System RTL entry point" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/setstring_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/setstring_builtin"
ASAN_OPTIONS=detect_leaks=1 "$tmp/setstring_builtin"

if ./mp -Furtl \
	-o"$tmp/setstring_non_string_rejected.cc" \
	tests/setstring_non_string_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "SetString accepted a non-string out destination" >&2
	exit 1
fi
if ! rg -Fq \
	"no matching overload for 'setstring'" \
	"$tmp/stderr"
then
	echo "SetString produced the wrong non-string diagnostic" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

echo "SetString builtin tests passed"
