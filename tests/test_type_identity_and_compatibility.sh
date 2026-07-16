#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-type-identity-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-Isrc \
	-Irtl \
	tests/type_conversion_algebra.cc \
	src/cst.o \
	src/directive_expr.o \
	src/frame.o \
	src/types.o \
	src/evaluator.o \
	src/builtins.o \
	src/units.o \
	src/emit.o \
	src/diagnostic.o \
	-o "$tmp/type_conversion_algebra"

"$tmp/type_conversion_algebra"

./mp -Furtl -o"$tmp/type_identity.cc" \
	tests/type_identity_and_compatibility.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/type_identity.cc" \
	"$tmp/system.cc" \
	-o "$tmp/type_identity"

ASAN_OPTIONS=detect_leaks=1 "$tmp/type_identity"

for kind in POINTER STRING SET RANGE ARRAY FILE ROUTINE CLASSREF
do
	if ./mp -Furtl -d"TEST_$kind" \
		-o"$tmp/carrier_collision.cc" \
		tests/type_carrier_collision_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted an erased C++ overload collision: $kind" >&2
		exit 1
	fi
	if ! rg -Fq 'same C++ parameter carriers' "$tmp/stderr"
	then
		echo "wrong C++ carrier-collision diagnostic: $kind" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi

	if ./mp -Furtl -d"TEST_$kind" \
		-o"$tmp/var_identity.cc" \
		tests/type_var_identity_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted distinct Type* as typed var: $kind" >&2
		exit 1
	fi
	if ! rg -Fq 'no matching overload' "$tmp/stderr"
	then
		echo "wrong typed-var identity diagnostic: $kind" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

if ./mp -Furtl -o"$tmp/accidental_override.cc" \
	tests/accidental_virtual_carrier_override_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an accidental C++ virtual override" >&2
	exit 1
fi
if ! rg -Fq 'would accidentally override an ancestor' "$tmp/stderr"
then
	echo "wrong accidental-override diagnostic" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "type identity and compatibility tests passed"
