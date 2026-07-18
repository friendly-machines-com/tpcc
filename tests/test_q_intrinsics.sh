#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-q-intrinsics.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/q_intrinsics.cc" \
	tests/q_intrinsics.pp

for operation in \
	p_abs m_unchecked_abs \
	p_succ m_unchecked_succ \
	p_pred m_unchecked_pred
do
	rg -Fq "::u_system::$operation" \
		"$tmp/q_intrinsics.cc"
done

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
	"$tmp/q_intrinsics.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/q_intrinsics"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/q_intrinsics"

./mp -Furtl \
	-o"$tmp/unchecked_constants.cc" \
	tests/q_intrinsics_unchecked_constants.pp
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
	"$tmp/unchecked_constants.cc" \
	"$tmp/system.cc" \
	-o "$tmp/unchecked_constants"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/unchecked_constants"

for define in TEST_ABS TEST_SUCC TEST_PRED
do
	if ./mp -Furtl \
		-d"$define" \
		-o"$tmp/checked_constant.cc" \
		tests/q_intrinsics_checked_constant.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "$define checked constant unexpectedly compiled" >&2
		exit 1
	fi
	rg -Fq 'integer constant overflow' \
		"$tmp/stderr"
done

echo "Q intrinsic tests passed"
