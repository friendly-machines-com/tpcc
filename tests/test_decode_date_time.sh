#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/decode_date_time.cc" \
	tests/decode_date_time.pp

if ! grep -Fq '::u_sysutils::p_decodedate' \
	"$tmp/decode_date_time.cc"
then
	echo "SysUtils.DecodeDate did not use p_decodedate" >&2
	exit 1
fi

if ! grep -Fq '::u_sysutils::p_decodetime' \
	"$tmp/decode_date_time.cc"
then
	echo "SysUtils.DecodeTime did not use p_decodetime" >&2
	exit 1
fi

tpcc_build "$tmp/decode_date_time" \
	"$tmp/decode_date_time.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/decode_date_time"

echo "DecodeDate and DecodeTime tests passed"
