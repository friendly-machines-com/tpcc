#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-q-directive-scoping.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/q_directive_scoping.cc" \
	tests/q_directive_scoping.pp

rg -q \
	'for \(p_loopunchecked = .*::u_system::m_unchecked_succ\(p_loopunchecked\)' \
	"$tmp/q_directive_scoping.cc"
rg -q \
	'for \(p_loopchecked = .*::u_system::p_succ\(p_loopchecked\)' \
	"$tmp/q_directive_scoping.cc"
rg -q \
	'for \(p_downunchecked = .*::u_system::m_unchecked_pred\(p_downunchecked\)' \
	"$tmp/q_directive_scoping.cc"
rg -q \
	'for \(p_downchecked = .*::u_system::p_pred\(p_downchecked\)' \
	"$tmp/q_directive_scoping.cc"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/q_directive_scoping.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/q_directive_scoping"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/q_directive_scoping"

./mp -Furtl \
	-o"$tmp/q_directive_scoping_constants.cc" \
	tests/q_directive_scoping_constants.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/q_directive_scoping_constants.cc" \
	"$tmp/system.cc" \
	-o "$tmp/q_directive_scoping_constants"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/q_directive_scoping_constants"

for define in TEST_ABS TEST_SUCC TEST_PRED
do
	if ./mp -Furtl \
		-d"$define" \
		-o"$tmp/checked_constant.cc" \
		tests/q_directive_scoping_checked_constant.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "$define checked outer call unexpectedly compiled" >&2
		exit 1
	fi
	rg -Fq 'integer constant overflow' \
		"$tmp/stderr"
done

echo "Q directive-scoping tests passed"
