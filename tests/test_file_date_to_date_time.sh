#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/file_date_to_date_time.cc" \
	tests/file_date_to_date_time.pp

if ! grep -Fq '::u_sysutils::p_filedatetodatetime' \
	"$tmp/file_date_to_date_time.cc"
then
	echo "SysUtils.FileDateToDateTime did not use p_filedatetodatetime" >&2
	exit 1
fi

tpcc_build "$tmp/file_date_to_date_time" \
	"$tmp/file_date_to_date_time.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

TZ=UTC0 \
tpcc_run \
	"$tmp/file_date_to_date_time"

tpcc_build "$tmp/file_date_to_date_time_runtime" \
	tests/file_date_to_date_time_runtime.cpp

tpcc_run \
	"$tmp/file_date_to_date_time_runtime"

echo "FileDateToDateTime tests passed"
