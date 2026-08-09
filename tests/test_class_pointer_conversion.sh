#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-class-pointer-conversion.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/class_pointer_conversion.cc" \
	tests/class_pointer_conversion.pp

for required in \
	'static_cast<::u_system::t_pointer>' \
	'p_returnpointer' \
	'p_takepointer' \
	'p_takeconstpointer'
do
	if ! rg -Fq "$required" \
		"$tmp/class_pointer_conversion.cc"
	then
		echo "missing class-to-Pointer lowering: $required" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/class_pointer_conversion.cc" \
	"$tmp/system.cc" \
	-o "$tmp/class_pointer_conversion"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/class_pointer_conversion"

for kind in \
	TYPED_POINTER \
	CLASSREF_TYPED_POINTER \
	VAR \
	OUT \
	OLD_OBJECT \
	INTERFACE \
	CHAIN
do
	if ./mp -Furtl -d"TEST_$kind" \
		-o"$tmp/rejected.cc" \
		tests/class_pointer_conversion_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted forbidden class-to-Pointer conversion: $kind" >&2
		exit 1
	fi
	if ! rg -q \
		'no implicit conversion|no matching overload' \
		"$tmp/stderr"
	then
		echo "wrong class-to-Pointer rejection diagnostic: $kind" >&2
		sed -n '1,100p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "class-to-Pointer conversion tests passed"
