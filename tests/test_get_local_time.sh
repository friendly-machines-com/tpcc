#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/get_local_time.cc" \
	tests/get_local_time.pp

if ! rg -Fq '::u_sysutils::p_getlocaltime' \
	"$tmp/get_local_time.cc"
then
	echo "SysUtils.GetLocalTime did not use p_getlocaltime" >&2
	exit 1
fi

tpcc_build "$tmp/get_local_time" \
	"$tmp/get_local_time.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/get_local_time"

tpcc_build "$tmp/get_local_time_runtime" \
	tests/get_local_time_runtime.cpp

tpcc_run \
	"$tmp/get_local_time_runtime"

echo "GetLocalTime tests passed"
