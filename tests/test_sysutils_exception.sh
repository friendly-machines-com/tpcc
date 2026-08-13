#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/sysutils_exception.cc" tests/sysutils_exception.pp
tpcc_build "$tmp/sysutils_exception" \
	"$tmp/sysutils_exception.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/sysutils_exception"

echo "SysUtils Exception tests passed"
