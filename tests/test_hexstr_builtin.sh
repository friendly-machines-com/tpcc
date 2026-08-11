#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-hexstr-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/hexstr_builtin.cc" tests/hexstr_builtin.pp
for required in \
	'::u_system::p_hexstr(p_l,' \
	'::u_system::p_hexstr(p_i,' \
	'::u_system::p_hexstr(p_q,' \
	'::u_system::p_hexstr(p_p)'
do
	if ! rg -Fq "$required" "$tmp/hexstr_builtin.cc"
	then
		echo "missing HexStr overload lowering: $required" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/hexstr_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/hexstr_builtin_pascal"
ASAN_OPTIONS=detect_leaks=1 "$tmp/hexstr_builtin_pascal"

echo "HexStr builtin tests passed"
