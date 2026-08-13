#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/implicit_ordinal_narrowing.cc" \
	tests/implicit_ordinal_narrowing.pp

grep -Fq '::u_system::m_ordinal_cast' \
	"$tmp/implicit_ordinal_narrowing.cc"
grep -Fq '::u_system::m_range_checked_ordinal_cast' \
	"$tmp/implicit_ordinal_narrowing.cc"

tpcc_build "$tmp/implicit_ordinal_narrowing" \
	"$tmp/implicit_ordinal_narrowing.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/implicit_ordinal_narrowing"

echo "implicit ordinal narrowing tests passed"
