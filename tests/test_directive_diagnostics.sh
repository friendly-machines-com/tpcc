#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-directive-diagnostics-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/directive_diagnostics_inactive.cc" \
	tests/directive_diagnostics_inactive.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-Irtl \
	-I"$tmp" \
	"$tmp/directive_diagnostics_inactive.cc" \
	"$tmp/system.cc" \
	-o "$tmp/directive_diagnostics_inactive"
"$tmp/directive_diagnostics_inactive"

for source in \
	tests/directive_error.pp \
	tests/directive_fatal.pp \
	tests/directive_error_empty.pp \
	tests/directive_error_include.pp
do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted active diagnostic directive: $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! rg -Fq -- "$expected" "$tmp/stderr"
	then
		echo "wrong directive diagnostic; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "directive diagnostic tests passed"
