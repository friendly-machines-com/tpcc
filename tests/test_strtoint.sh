#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/strtoint.cc" tests/strtoint.pp

if ! grep -Fq '::u_sysutils::p_strtoint(' "$tmp/strtoint.cc"; then
	echo "SysUtils.StrToInt did not resolve in the SysUtils namespace" >&2
	exit 1
fi
if ! grep -Fq '::u_system::p_val(' "$tmp/sysutils.cc"; then
	echo "SysUtils.StrToInt did not delegate parsing to System.Val" >&2
	exit 1
fi

tpcc_build "$tmp/strtoint" \
	"$tmp/strtoint.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/strtoint"

echo "StrToInt tests passed"
