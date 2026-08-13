#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/implicit_real_narrowing.cc" \
	tests/implicit_real_narrowing.pp

grep -Fq '::u_system::m_real_cast' \
	"$tmp/implicit_real_narrowing.cc"
grep -Fq '::u_system::m_range_checked_real_cast' \
	"$tmp/implicit_real_narrowing.cc"

tpcc_build "$tmp/implicit_real_narrowing" \
	"$tmp/implicit_real_narrowing.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/implicit_real_narrowing"

if tpcc_translate -dCHECK_CONSTANT_REJECTION \
	-o"$tmp/constant_rejected.cc" \
	tests/implicit_real_narrowing.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an out-of-range checked real constant" >&2
	exit 1
fi

if ! grep -Fq \
	'real constant out of range for target type' \
	"$tmp/stderr"
then
	echo "wrong checked real constant diagnostic" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

echo "implicit real narrowing tests passed"
