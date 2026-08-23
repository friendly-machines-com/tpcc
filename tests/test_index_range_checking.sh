#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/index_range_checking.cc" \
	tests/index_range_checking.pp

grep -Fq '::u_system::m_unchecked_index' \
	"$tmp/index_range_checking.cc"
grep -Fq '::u_system::p_index' \
	"$tmp/index_range_checking.cc"

tpcc_build "$tmp/index_range_checking" \
	"$tmp/index_range_checking.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/index_range_checking"

echo "index range-checking tests passed"
