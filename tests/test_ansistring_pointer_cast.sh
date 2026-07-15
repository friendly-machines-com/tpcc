#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-ansistring-pointer-cast-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/ansistring_pointer_cast.cc" \
	tests/ansistring_pointer_cast.pp

if ! rg -Fq '.m_pointer()' \
	"$tmp/ansistring_pointer_cast.cc"
then
	echo "AnsiString cast did not use its data-pointer operation" >&2
	exit 1
fi
if rg -Fq \
	'static_cast<::u_system::t_pointer>(p_s)' \
	"$tmp/ansistring_pointer_cast.cc"
then
	echo "AnsiString was cast as an aggregate rather than its data address" >&2
	exit 1
fi
if ! rg -Fq \
	'reinterpret_cast<::u_system::t_byte*>(::u_system::p_add' \
	"$tmp/ansistring_pointer_cast.cc"
then
	echo "pointer-sized integer did not convert to a typed pointer" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/ansistring_pointer_cast.cc" \
	"$tmp/system.cc" \
	-o "$tmp/ansistring_pointer_cast"

actual=$(ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/ansistring_pointer_cast")
if test "$actual" != 'XYc'
then
	echo "unexpected AnsiString pointer-cast result: $actual" >&2
	exit 1
fi

echo "AnsiString pointer-cast tests passed"
