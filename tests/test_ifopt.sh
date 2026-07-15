#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-ifopt-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/ifopt.cc" tests/ifopt.pp

for selected in \
	p_qplus \
	p_qminusrejected \
	p_outerstatepreserved \
	p_qminus \
	p_rinitiallyminus \
	p_nestedactive
do
	if ! rg -q "\\b${selected}\\b" "$tmp/ifopt.cc"; then
		echo "missing IFOPT-selected declaration: $selected" >&2
		exit 1
	fi
done

if rg -q 'p_wrong' "$tmp/ifopt.cc"; then
	echo "an unselected IFOPT branch was emitted" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/ifopt.cc" \
	"$tmp/system.cc" \
	-o "$tmp/ifopt"
ASAN_OPTIONS=detect_leaks=1 "$tmp/ifopt"

if ./mp -Furtl -o"$tmp/invalid.cc" \
	tests/ifopt_invalid.pp >"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "expected malformed IFOPT to be rejected" >&2
	exit 1
fi
if ! rg -Fq \
	'$ifopt expects one option letter followed by + or -' \
	"$tmp/stderr"
then
	echo "wrong malformed-IFOPT diagnostic" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "IFOPT tests passed"
