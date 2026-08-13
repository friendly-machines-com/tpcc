#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/paramstr_builtin.cc" \
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

tpcc_build "$tmp/paramstr_builtin" \
	"$tmp/paramstr_builtin.cc" \
	"$tmp/system.cc"

long_argument='xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'
tpcc_run \
	"$tmp/paramstr_builtin" \
	"$tmp/paramstr_builtin" \
	alpha \
	'two words' \
	'' \
	"$long_argument"

echo "ParamStr builtin tests passed"
