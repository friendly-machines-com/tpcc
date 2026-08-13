#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/overflow_checking.cc" \
	tests/overflow_checking.pp

rg -Fq '::u_system::o_unchecked_add' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_add' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_unchecked_negative' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_negative' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_unchecked_intdivide' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_intdivide' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_unchecked_subtract' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_subtract' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_unchecked_multiply' \
	"$tmp/overflow_checking.cc"
rg -Fq '::u_system::o_multiply' \
	"$tmp/overflow_checking.cc"

tpcc_build "$tmp/overflow_checking" \
	"$tmp/overflow_checking.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/overflow_checking"

echo "overflow-checking tests passed"
