#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/single_type.cc" \
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

tpcc_build "$tmp/single_type" \
	tests/single_type_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/single_type"

echo "Single type tests passed"
