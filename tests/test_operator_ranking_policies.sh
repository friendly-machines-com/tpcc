#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-operator-ranking-policies.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/operator_ranking_policies.cc" \
	tests/operator_ranking_policies.pp

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
	"$tmp/operator_ranking_policies.cc" \
	"$tmp/system.cc" \
	-o "$tmp/operator_ranking_policies"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/operator_ranking_policies"

for policy in \
	ENUM_ARITHMETIC \
	ENUM_BITWISE \
	ENUM_SHIFT \
	CHAR_ARITHMETIC \
	ARRAY_CONCATENATION
do
	if ./mp -Furtl -d"$policy" \
		-o"$tmp/rejected.cc" \
		tests/operator_ranking_policy_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted forbidden operator policy: $policy" >&2
		exit 1
	fi
	if ! rg -Fq 'no matching overload' "$tmp/stderr"
	then
		echo "wrong forbidden-operator diagnostic: $policy" >&2
		sed -n '1,140p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "operator ranking policy tests passed"
