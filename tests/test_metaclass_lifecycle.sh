#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-metaclass-lifecycle-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/metaclass_lifecycle.cc" \
	tests/metaclass_lifecycle.pp

if test "$(rg -F -c 'void m_init();' \
	"$tmp/metaclass_lifecycle.cc")" -ne 2
then
	echo "class constructors were not emitted exactly once per declaring metaclass" >&2
	exit 1
fi

if test "$(rg -F -c 'void p_initialize();' \
	"$tmp/metaclass_lifecycle.cc")" -ne 1
then
	echo "class constructor did not remain separate from its same-named ordinary method" >&2
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
	"$tmp/metaclass_lifecycle.cc" \
	"$tmp/system.cc" \
	-o "$tmp/metaclass_lifecycle"

actual=$("$tmp/metaclass_lifecycle")
expected='base
child
11'
if test "$actual" != "$expected"
then
	echo "unexpected class-constructor order or class-variable value" >&2
	printf 'expected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
	exit 1
fi

echo "metaclass lifecycle tests passed"
