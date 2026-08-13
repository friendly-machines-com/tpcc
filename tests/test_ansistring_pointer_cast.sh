#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/system.cc" rtl/system.pp
tpcc_translate -o"$tmp/ansistring_pointer_cast.cc" \
	tests/ansistring_pointer_cast.pp

if ! grep -Fq '.m_pointer()' \
	"$tmp/ansistring_pointer_cast.cc"
then
	echo "AnsiString cast did not use its data-pointer operation" >&2
	exit 1
fi
if grep -Fq \
	'static_cast<::u_system::t_pointer>(p_s)' \
	"$tmp/ansistring_pointer_cast.cc"
then
	echo "AnsiString was cast as an aggregate rather than its data address" >&2
	exit 1
fi
if ! grep -Fq \
	'reinterpret_cast<::u_system::t_byte*>(::u_system::o_unchecked_add' \
	"$tmp/ansistring_pointer_cast.cc"
then
	echo "pointer-sized integer did not convert to a typed pointer" >&2
	exit 1
fi

tpcc_build "$tmp/ansistring_pointer_cast" \
	"$tmp/ansistring_pointer_cast.cc" \
	"$tmp/system.cc"

actual=$(tpcc_run \
	"$tmp/ansistring_pointer_cast")
if test "$actual" != 'XYc'
then
	echo "unexpected AnsiString pointer-cast result: $actual" >&2
	exit 1
fi

echo "AnsiString pointer-cast tests passed"
