#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/hexstr_builtin.cc" tests/hexstr_builtin.pp
for required in \
	'::u_system::p_hexstr(p_l,' \
	'::u_system::p_hexstr(p_i,' \
	'::u_system::p_hexstr(p_q,' \
	'::u_system::p_hexstr(p_p)'
do
	if ! grep -Fq "$required" "$tmp/hexstr_builtin.cc"
	then
		echo "missing HexStr overload lowering: $required" >&2
		exit 1
	fi
done

tpcc_build "$tmp/hexstr_builtin_pascal" \
	"$tmp/hexstr_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/hexstr_builtin_pascal"

echo "HexStr builtin tests passed"
