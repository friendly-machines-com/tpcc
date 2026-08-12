#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-typed-integer-constant-conversion.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/typed_integer_constant_conversion.cc" \
	tests/typed_integer_constant_conversion.pp

# R+ must not emit a range check after semantic constant evaluation has
# already proved that the actual value belongs to the selected destination.
if rg -Fq 'm_range_checked_ordinal_cast' \
	"$tmp/typed_integer_constant_conversion.cc"
then
	echo "range-checked a proved in-range typed integer constant" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/typed_integer_constant_conversion.cc" \
	"$tmp/system.cc" \
	-o "$tmp/typed_integer_constant_conversion"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/typed_integer_constant_conversion"

if ./mp -Furtl \
	-o"$tmp/nonconstant.cc" \
	tests/typed_integer_nonconstant_common_domain.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted incomparable runtime Int64/QWord division domains" >&2
	exit 1
fi

rg -Fq "ambiguous overload for 'uncheckedintdivide'" "$tmp/stderr"
rg -Fq 'conflicting argument preferences:' "$tmp/stderr"

echo "typed integer constant conversion tests passed"
