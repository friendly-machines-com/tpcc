#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-paramstr-builtin-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/paramstr_builtin.cc" \
	tests/paramstr_builtin.pp

for required in \
	'int main(int argc, char* argv[])' \
	'::u_system::m_set_program_arguments(argc, argv);' \
	'::u_system::p_paramstr(' \
	'::u_system::p_paramcount()'
do
	if ! rg -Fq "$required" "$tmp/paramstr_builtin.cc"
	then
		echo "missing ParamStr lowering: $required" >&2
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
	"$tmp/paramstr_builtin.cc" \
	"$tmp/system.cc" \
	-o "$tmp/paramstr_builtin"

long_argument='xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/paramstr_builtin" \
	"$tmp/paramstr_builtin" \
	alpha \
	'two words' \
	'' \
	"$long_argument"

echo "ParamStr builtin tests passed"
