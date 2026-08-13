#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/inttostr.cc" tests/inttostr.pp

if ! rg -Fq '::u_sysutils::p_inttostr(' "$tmp/inttostr.cc"; then
	echo "SysUtils.IntToStr did not resolve in the SysUtils namespace" >&2
	exit 1
fi
if [ "$(rg -Fc '::u_system::p_str(' "$tmp/sysutils.cc")" -lt 3 ]; then
	echo "SysUtils.IntToStr overloads did not delegate to System.Str" >&2
	exit 1
fi

tpcc_build "$tmp/inttostr" \
	"$tmp/inttostr.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/inttostr"

echo "IntToStr tests passed"
