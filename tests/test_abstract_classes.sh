#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-abstract-class-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/abstract_classes.cc" \
	tests/abstract_classes.pp

if rg -q '\) = 0;' "$tmp/abstract_classes.cc" ||
   rg -Fq 'p_runerror(' "$tmp/abstract_classes.cc"
then
	echo "class abstract incorrectly changed C++ class or method emission" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-Irtl \
	-I"$tmp" \
	"$tmp/abstract_classes.cc" \
	"$tmp/system.cc" \
	-o "$tmp/abstract_classes"

actual=$("$tmp/abstract_classes")
expected='concrete
concrete
concrete'
if test "$actual" != "$expected"
then
	echo "class abstract changed native construction behavior" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

echo "abstract class tests passed"
