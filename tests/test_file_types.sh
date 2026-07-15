#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-file-types-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/file_types.cc" tests/file_types.pp

if ! rg -Fq '::u_system::t_file p_binaryfile;' "$tmp/file_types.cc"
then
	echo "File did not emit the untyped binary-file carrier" >&2
	exit 1
fi
if ! rg -Fq '::u_system::t_text p_textalias;' "$tmp/file_types.cc"
then
	echo "TextFile did not alias Text" >&2
	exit 1
fi
if ! rg -Fq \
	'::u_system::t_typedfile<::u_system::t_integer> p_integers;' \
	"$tmp/file_types.cc"
then
	echo "file of Integer did not emit a parameterized typed-file carrier" >&2
	exit 1
fi
if ! rg -Fq \
	'::u_system::t_typedfile<t_tnode> p_nodes;' \
	"$tmp/file_types.cc"
then
	echo "file-of forward element type was not resolved" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/file_types.cc" \
	rtl/system.cc \
	-o "$tmp/file_types"
ASAN_OPTIONS=detect_leaks=1 "$tmp/file_types"

if ./mp -Furtl -o"$tmp/mismatch.cc" \
	tests/file_types_mismatch.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted file of Byte as file of Integer" >&2
	exit 1
fi
if ! rg -Fq 'no matching overload' "$tmp/stderr"
then
	echo "wrong diagnostic for incompatible typed files" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "file type tests passed"
