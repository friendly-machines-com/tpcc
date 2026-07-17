#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-single-type-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/single_type.cc" \
	tests/single_type.pp

if ! rg -q '::u_system::t_single p_s;' "$tmp/single_type.cc"
then
	echo "Single did not lower to ::u_system::t_single" >&2
	exit 1
fi
if ! rg -q \
	'p_coerced = ::u_system::m_real_cast<::u_system::t_single>\(p_e\);' \
	"$tmp/single_type.cc"
then
	echo "numeric Coerce did not lower to the defined unchecked real cast" >&2
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
	tests/single_type_runtime.cc \
	"$tmp/system.cc" \
	-o "$tmp/single_type"
ASAN_OPTIONS=detect_leaks=1 "$tmp/single_type"

echo "Single type tests passed"
