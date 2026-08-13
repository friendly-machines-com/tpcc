#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/strpas.cc" tests/strpas.pp

if ! grep -Fq '::u_sysutils::p_strpas(' "$tmp/strpas.cc"; then
	echo "SysUtils.StrPas did not resolve in the SysUtils namespace" >&2
	exit 1
fi
if ! grep -Fq '::u_system::o_implicit(' "$tmp/sysutils.cc"; then
	echo "SysUtils.StrPas did not use the existing PChar-to-AnsiString conversion" >&2
	exit 1
fi

tpcc_build "$tmp/strpas" \
	"$tmp/strpas.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/strpas"

echo "StrPas tests passed"
