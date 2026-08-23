#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/real_constant_semantics.cc" \
	tests/real_constant_semantics.pp

tpcc_translate -dCPUX86_64 \
	-o"$tmp/x86_64.cc" \
	tests/real_constant_semantics.pp
tpcc_translate -dCPUAARCH64 \
	-o"$tmp/aarch64.cc" \
	tests/real_constant_semantics.pp
cmp "$tmp/x86_64.cc" "$tmp/aarch64.cc"

tpcc_build "$tmp/real_constant_semantics" \
	tests/real_constant_semantics_runtime.cpp \
	"$tmp/system.cc"

"$tmp/real_constant_semantics"

tpcc_translate -dEXECUTE_UNCHECKED_HUGE \
	-o"$tmp/real_constant_unchecked_huge.cc" \
	tests/real_constant_semantics.pp
tpcc_build "$tmp/real_constant_unchecked_huge" \
	'-DTPCC_TEST_GENERATED_PROGRAM="real_constant_unchecked_huge.cc"' \
	-DTPCC_EXPECT_HUGE \
	tests/real_constant_semantics_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/real_constant_unchecked_huge"

tpcc_translate -dEXECUTE_CHECKED_HUGE \
	-o"$tmp/real_constant_checked_huge.cc" \
	tests/real_constant_semantics.pp
tpcc_build "$tmp/real_constant_checked_huge" \
	'-DTPCC_TEST_GENERATED_PROGRAM="real_constant_checked_huge.cc"' \
	-DTPCC_EXPECT_HUGE \
	tests/real_constant_semantics_runtime.cpp \
	"$tmp/system.cc"
if tpcc_run "$tmp/real_constant_checked_huge"; then
	echo "checked out-of-range real origin did not fail" >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 201 ]; then
	echo "checked out-of-range real origin returned $status rather than runtime error 201" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/out_of_domain.cc" \
	tests/real_constant_out_of_domain.pp \
	>"$tmp/out" 2>"$tmp/err"
then
	echo "accepted integer origin below Int64 minimum" >&2
	exit 1
fi

grep -Fq 'integer constant is below Int64 minimum' "$tmp/err"

echo "real constant semantics tests passed"
