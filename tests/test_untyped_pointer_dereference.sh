#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-untyped-pointer-dereference-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/untyped_pointer_dereference.cc" \
	tests/untyped_pointer_dereference.pp

if ! rg -Fq \
	'::u_system::tpcc_dereference_storage(p_rawdestination)' \
	"$tmp/untyped_pointer_dereference.cc"
then
	echo "untyped Pointer^ did not produce a raw storage place" >&2
	exit 1
fi
if rg -Fq '*p_rawdestination' \
	"$tmp/untyped_pointer_dereference.cc"
then
	echo "untyped Pointer^ was emitted as C++ unary dereference" >&2
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
	"$tmp/untyped_pointer_dereference.cc" \
	"$tmp/system.cc" \
	-o "$tmp/untyped_pointer_dereference"

actual=$(ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/untyped_pointer_dereference")
expected='42
17'
if test "$actual" != "$expected"
then
	echo "unexpected untyped Pointer^ result" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

echo "untyped pointer dereference tests passed"
