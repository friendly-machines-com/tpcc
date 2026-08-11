#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-shortstring-type-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/shortstring_type.cc" \
	tests/shortstring_type.pp

for expected in \
	'::u_system::t_shortstring<2> p_tiny;' \
	'::u_system::t_shortstring<5> p_name;' \
	'::u_system::t_shortstring<255> p_ordinary;' \
	'::u_system::tpcc_shortstring_from_c<2>' \
	'::u_system::tpcc_shortstring_from_c<5>' \
	'::u_system::tpcc_shortstring_cast<2>' \
	'::u_system::tpcc_shortstring_cast<5>' \
	'::u_system::p_setlength(' \
	'sizeof(::u_system::t_shortstring<2>)' \
	'sizeof(::u_system::t_shortstring<5>)' \
	'sizeof(::u_system::t_shortstring<255>)'
do
	if ! rg -Fq "$expected" "$tmp/shortstring_type.cc"; then
		echo "missing ShortString lowering: $expected" >&2
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
	"$tmp/shortstring_type.cc" \
	"$tmp/system.cc" \
	-o "$tmp/shortstring_type_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/shortstring_type_pascal"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	tests/shortstring_type_runtime.cc \
	-o "$tmp/shortstring_type_runtime"
ASAN_OPTIONS=detect_leaks=1 "$tmp/shortstring_type_runtime"

./mp -Furtl \
	-o"$tmp/implicit_narrowing.cc" \
	tests/ansistring_shortstring_implicit.pp
if ! rg -Fq \
	'::u_system::tpcc_shortstring_cast<3>' \
	"$tmp/implicit_narrowing.cc"
then
	echo "missing implicit AnsiString-to-ShortString narrowing" >&2
	exit 1
fi
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/implicit_narrowing.cc" \
	"$tmp/system.cc" \
	-o "$tmp/implicit_narrowing"
ASAN_OPTIONS=detect_leaks=1 "$tmp/implicit_narrowing"

./mp -Furtl \
	-o"$tmp/pchar_ansistring.cc" \
	tests/pchar_ansistring_implicit.pp
if ! rg -Fq \
	'::u_system::o_implicit(p_pointervalue, ::u_system::m_conversion_target<::u_system::t_ansistring>{})' \
	"$tmp/pchar_ansistring.cc"
then
	echo "missing direct PChar-to-AnsiString conversion" >&2
	exit 1
fi
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/pchar_ansistring.cc" \
	"$tmp/system.cc" \
	-o "$tmp/pchar_ansistring"
ASAN_OPTIONS=detect_leaks=1 "$tmp/pchar_ansistring"

# PChar has no bounded payload length, so the ShortString narrowing conversion
# remains unavailable until its truncation/range-check contract is specified.
if ./mp -Furtl \
	-o"$tmp/pchar_shortstring.cc" \
	tests/pchar_shortstring_implicit_rejected.pp \
	>"$tmp/pchar_shortstring.out" \
	2>"$tmp/pchar_shortstring.err"
then
	echo "accepted implicit PChar-to-ShortString conversion" >&2
	exit 1
fi

echo "ShortString type tests passed"
