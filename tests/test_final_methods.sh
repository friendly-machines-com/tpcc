#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-final-method-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/final_methods.cc" \
	tests/final_methods.pp

if test "$(rg -F -c ' override final;' \
	"$tmp/final_methods.cc")" -ne 2
then
	echo "instance and class override-final declarations were not emitted" >&2
	exit 1
fi
if ! rg -Fq 'virtual void p_directfinal() final;' \
	"$tmp/final_methods.cc"
then
	echo "direct virtual-final declaration was not emitted" >&2
	exit 1
fi
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Irtl \
	-I"$tmp" \
	"$tmp/final_methods.cc" \
	"$tmp/system.cc" \
	-o "$tmp/final_methods"
"$tmp/final_methods"

for define in TEST_INSTANCE TEST_CLASS
do
	if ./mp -Furtl -d"$define" \
		-o"$tmp/rejected.cc" \
		tests/final_method_override_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted an override of a final Pascal method: $define" >&2
		exit 1
	fi
	if ! rg -Fq 'overrides a final method' "$tmp/stderr"
	then
		echo "wrong final-override diagnostic: $define" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

if ./mp -Furtl \
	-o"$tmp/rejected.cc" \
	tests/final_nonvirtual_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted final on a nonvirtual method" >&2
	exit 1
fi
expected=$(sed -n '1p' \
	tests/final_nonvirtual_rejected.error)
if ! rg -Fq -- "$expected" "$tmp/stderr"
then
	echo "wrong nonvirtual-final diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "final method tests passed"
