#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/class_reference_constant.cc" \
	tests/class_reference_constant.pp

# The class-reference typed constant must be initialized by the constexpr
# metaclass accessor, making it a constant-initialized C++ global.
if ! grep "p_cfoo" "$tmp/class_reference_constant.cc" | grep -Fq 'p_classtype()'; then
	echo "class-reference constant was not initialized by the metaclass accessor" >&2
	exit 1
fi

tpcc_build "$tmp/class_reference_constant" \
	"$tmp/class_reference_constant.cc" \
	"$tmp/system.cc"
actual=$("$tmp/class_reference_constant")
expected='t_tfoo
t_tfoo
inherits'
if test "$actual" != "$expected"
then
	echo "class-reference constant dispatch failed" >&2
	printf 'expected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
	exit 1
fi

echo "class-reference constant tests passed"
