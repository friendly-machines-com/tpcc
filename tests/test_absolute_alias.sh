#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-absolute-alias-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/absolute_alias.cc" tests/absolute_alias.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-fno-strict-aliasing \
	-Irtl \
	-I"$tmp" \
	"$tmp/absolute_alias.cc" \
	"$tmp/system.cc" \
	-o "$tmp/absolute_alias"
ASAN_OPTIONS=detect_leaks=1 "$tmp/absolute_alias"

for source in \
	tests/absolute_var_param_rejected.pp \
	tests/absolute_non_pointer_rejected.pp \
	tests/absolute_size_mismatch_rejected.pp
do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid absolute source: $source" >&2
		exit 1
	fi
	expected_error=$(sed -n '1p' "$base.error")
	if ! rg -Fq -- "$expected_error" "$tmp/stderr"
	then
		echo "wrong absolute diagnostic; expected: $expected_error" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "Absolute alias tests passed"
