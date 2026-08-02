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
	-Werror \
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
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	tests/shortstring_type_runtime.cc \
	-o "$tmp/shortstring_type_runtime"
ASAN_OPTIONS=detect_leaks=1 "$tmp/shortstring_type_runtime"

if ./mp -Furtl \
	-o"$tmp/implicit_rejected.cc" \
	tests/ansistring_shortstring_implicit_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted implicit AnsiString-to-ShortString call conversion" >&2
	exit 1
fi
if ! rg -Fq \
	"no matching overload for 'taketiny'" \
	"$tmp/stderr"
then
	echo "wrong implicit AnsiString-to-ShortString diagnostic" >&2
	sed -n '1,120p' "$tmp/stderr" >&2
	exit 1
fi

echo "ShortString type tests passed"
