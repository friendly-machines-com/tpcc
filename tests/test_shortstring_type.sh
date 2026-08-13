#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/shortstring_type.cc" \
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

tpcc_build "$tmp/shortstring_type_pascal" \
	"$tmp/shortstring_type.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/shortstring_type_pascal"

tpcc_build "$tmp/shortstring_type_runtime" \
	tests/shortstring_type_runtime.cpp
tpcc_run "$tmp/shortstring_type_runtime"

tpcc_translate \
	-o"$tmp/implicit_narrowing.cc" \
	tests/ansistring_shortstring_implicit.pp
if ! rg -Fq \
	'::u_system::tpcc_shortstring_cast<3>' \
	"$tmp/implicit_narrowing.cc"
then
	echo "missing implicit AnsiString-to-ShortString narrowing" >&2
	exit 1
fi
tpcc_build "$tmp/implicit_narrowing" \
	"$tmp/implicit_narrowing.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/implicit_narrowing"

tpcc_translate \
	-o"$tmp/pchar_ansistring.cc" \
	tests/pchar_ansistring_implicit.pp
if ! rg -Fq \
	'::u_system::o_implicit(p_pointervalue, ::u_system::m_conversion_target<::u_system::t_ansistring>{})' \
	"$tmp/pchar_ansistring.cc"
then
	echo "missing direct PChar-to-AnsiString conversion" >&2
	exit 1
fi
tpcc_build "$tmp/pchar_ansistring" \
	"$tmp/pchar_ansistring.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/pchar_ansistring"

# PChar has no bounded payload length, so the ShortString narrowing conversion
# remains unavailable until its truncation/range-check contract is specified.
if tpcc_translate \
	-o"$tmp/pchar_shortstring.cc" \
	tests/pchar_shortstring_implicit_rejected.pp \
	>"$tmp/pchar_shortstring.out" \
	2>"$tmp/pchar_shortstring.err"
then
	echo "accepted implicit PChar-to-ShortString conversion" >&2
	exit 1
fi

echo "ShortString type tests passed"
