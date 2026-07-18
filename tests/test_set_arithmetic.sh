#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-set-arithmetic.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/set_arithmetic.cc" \
	tests/set_arithmetic.pp

for operation in \
	'::u_system::o_unchecked_add(' \
	'::u_system::o_unchecked_subtract(' \
	'::u_system::o_add(' \
	'::u_system::o_subtract('
do
	if ! rg -Fq "$operation" \
		"$tmp/set_arithmetic.cc"
	then
		echo "missing predefined set operation: $operation" >&2
		exit 1
	fi
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
	"$tmp/set_arithmetic.cc" \
	"$tmp/system.cc" \
	-o "$tmp/set_arithmetic"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/set_arithmetic"

if ./mp -Furtl -o"$tmp/rejected.cc" \
	tests/set_arithmetic_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted set arithmetic with incompatible item domains" >&2
	exit 1
fi
for required in \
	"no matching overload for 'uncheckedadd'" \
	'arg 1:' \
	'arg 2:' \
	'all candidates:'
do
	if ! rg -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete incompatible-set diagnostic" >&2
		sed -n '1,180p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "set arithmetic tests passed"
