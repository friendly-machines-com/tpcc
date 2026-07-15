#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-metaclass-construction-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/construction.cc" \
	tests/metaclass_construction.pp

if ! rg -Fq 'void t_tbase::p_create(' \
	"$tmp/construction.cc"
then
	echo "ordinary constructor body was not emitted as a Unit initializer" >&2
	exit 1
fi
if rg -Fq 'return this;' "$tmp/construction.cc"
then
	echo "ordinary constructor initializer still returns this" >&2
	exit 1
fi
if ! rg -Fq '::u_system::m_construct<t_tbase' \
	"$tmp/construction.cc"
then
	echo "class-reference constructor call did not emit Construct" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-I"$tmp" \
	-Irtl \
	"$tmp/construction.cc" \
	"$tmp/system.cc" \
	-o "$tmp/construction"

actual=$("$tmp/construction")
expected='7
2
1'
if test "$actual" != "$expected"
then
	echo "unexpected dynamic construction result" >&2
	printf 'expected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
	exit 1
fi

echo "metaclass construction tests passed"
