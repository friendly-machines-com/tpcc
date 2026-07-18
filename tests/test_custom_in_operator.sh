#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-custom-in-operator.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/custom_in_operator.cc" \
	tests/custom_in_operator.pp

if ! rg -Fq 'o_in(' "$tmp/custom_in_operator.cc"
then
	echo "custom In did not lower through its ordinary operator declaration" >&2
	exit 1
fi
if ! rg -Fq '::u_system::o_in(' "$tmp/custom_in_operator.cc"
then
	echo "System set membership fallback was not retained" >&2
	exit 1
fi

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
	"$tmp/custom_in_operator.cc" \
	"$tmp/system.cc" \
	-o "$tmp/custom_in_operator"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/custom_in_operator"

for rejection in ITEM CONTAINER
do
	if ./mp -Furtl -dREJECT_"$rejection" \
		-o"$tmp/rejected.cc" \
		tests/custom_in_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid In operands: $rejection" >&2
		exit 1
	fi
	for required in \
		"no matching overload for 'in'" \
		'arg 1:' \
		'arg 2:' \
		'all candidates:'
	do
		if ! rg -Fq "$required" "$tmp/stderr"
		then
			echo "incomplete In diagnostic for $rejection" >&2
			sed -n '1,180p' "$tmp/stderr" >&2
			exit 1
		fi
	done
done

echo "custom In operator tests passed"
