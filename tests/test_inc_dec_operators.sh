#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-inc-dec-operators.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/inc_dec_operators.cc" \
	tests/inc_dec_operators.pp

for operation in \
	o_inc o_unchecked_inc o_dec o_unchecked_dec \
	o_add o_unchecked_add o_subtract o_unchecked_subtract
do
	rg -Fq "$operation" \
		"$tmp/inc_dec_operators.cc"
done

${CXX:-g++} \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/inc_dec_operators.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/inc_dec_operators"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/inc_dec_operators"

check_rejected()
{
	define=$1
	expected=$2
	if ./mp -Furtl \
		-d"$define" \
		-o"$tmp/rejected.cc" \
		tests/inc_dec_operators.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "expected $define mode to be rejected" >&2
		exit 1
	fi
	if ! rg -Fq -- "$expected" "$tmp/stderr"; then
		echo "wrong diagnostic for $define; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
}

check_rejected \
	TEST_INC_NON_PLACE \
	"inc destination is not assignable"
check_rejected \
	TEST_INC_WRITE_ONLY \
	"write-only property 'value' cannot be read"
check_rejected \
	TEST_POINTER_REAL_DISTANCE \
	"no matching overload for 'uncheckedadd'"

echo "inc/dec operator tests passed"
