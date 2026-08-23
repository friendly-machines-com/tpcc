#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

tpcc_translate -o"$tmp/currency_type.cc" tests/currency_type.pp
tpcc_build "$tmp/currency_type" \
	tests/currency_type_runtime.cpp \
	"$tmp/system.cc"
output=$(tpcc_run "$tmp/currency_type")
if [ "$output" != "value_currency = 123.4567" ]; then
	echo "Currency WriteLn produced unexpected text: $output" >&2
	exit 1
fi

tpcc_translate -o"$tmp/currency_range_modes.cc" \
	tests/currency_range_modes.pp
tpcc_build "$tmp/currency_range_modes" \
	tests/currency_range_modes_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/currency_range_modes"

tpcc_translate -dEXECUTE_CHECKED \
	-o"$tmp/currency_range_modes_checked.cc" \
	tests/currency_range_modes.pp
tpcc_build "$tmp/currency_range_modes_checked" \
	'-DTPCC_TEST_GENERATED_PROGRAM="currency_range_modes_checked.cc"' \
	tests/currency_range_modes_runtime.cpp \
	"$tmp/system.cc"
if tpcc_run "$tmp/currency_range_modes_checked"; then
	echo "checked out-of-range Currency origin did not fail" >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 201 ]; then
	echo "checked out-of-range Currency origin returned $status rather than runtime error 201" >&2
	exit 1
fi

tpcc_translate -dEXECUTE_CHECKED_TYPED \
	-o"$tmp/currency_range_modes_checked_typed.cc" \
	tests/currency_range_modes.pp
tpcc_build "$tmp/currency_range_modes_checked_typed" \
	'-DTPCC_TEST_GENERATED_PROGRAM="currency_range_modes_checked_typed.cc"' \
	tests/currency_range_modes_runtime.cpp \
	"$tmp/system.cc"
if tpcc_run "$tmp/currency_range_modes_checked_typed"; then
	echo "checked out-of-range typed Currency conversion did not fail" >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 201 ]; then
	echo "checked out-of-range typed Currency conversion returned $status rather than runtime error 201" >&2
	exit 1
fi

tpcc_translate -o"$tmp/currency_q_modes.cc" tests/currency_q_modes.pp
tpcc_build "$tmp/currency_q_modes" \
	tests/currency_q_modes_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/currency_q_modes"

tpcc_translate -dEXECUTE_CHECKED \
	-o"$tmp/currency_q_modes_checked.cc" \
	tests/currency_q_modes.pp
tpcc_build "$tmp/currency_q_modes_checked" \
	'-DTPCC_TEST_GENERATED_PROGRAM="currency_q_modes_checked.cc"' \
	tests/currency_q_modes_runtime.cpp \
	"$tmp/system.cc"
if tpcc_run "$tmp/currency_q_modes_checked"; then
	echo "checked Currency arithmetic did not fail" >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 215 ]; then
	echo "checked Currency arithmetic returned $status rather than runtime error 215" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/currency_checked_arithmetic_rejected.cc" \
	tests/currency_checked_arithmetic_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted a folded overflowing checked Currency operation" >&2
	exit 1
fi
grep -Fq "Currency constant overflow" "$tmp/stderr"

if tpcc_translate -o"$tmp/currency_checked_constant_rejected.cc" \
	tests/currency_checked_constant_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an out-of-range R+ Currency constant" >&2
	exit 1
fi
grep -Fq "constant out of range for target type" "$tmp/stderr"

if tpcc_translate -o"$tmp/currency_common_domain_ambiguous.cc" \
	tests/currency_common_domain_ambiguous.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted incomparable Currency common domains" >&2
	exit 1
fi
if [ "$(grep -c "ambiguous overload" "$tmp/stderr")" -ne 18 ]; then
	echo "did not report every operand-order Currency ambiguity" >&2
	cat "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/currency_raw_field_rejected.cc" \
	tests/currency_raw_field_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "exposed Currency's representation field to Pascal source" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/packed_currency_array_rejected.cc" \
	tests/packed_currency_array_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted a Currency array inside a packed record" >&2
	exit 1
fi
grep -Fq "alignment != 1" "$tmp/stderr"

if tpcc_translate -o"$tmp/ordinal.cc" \
	tests/currency_ordinal_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted Currency as an ordinal value" >&2
	exit 1
fi
grep -Fq "requires an ordinal argument" "$tmp/stderr"

if tpcc_translate -o"$tmp/div.cc" \
	tests/currency_div_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted Currency for integer division" >&2
	exit 1
fi
grep -Fq "intdivide'" "$tmp/stderr"

for mode in unchecked checked
do
	definition=
	if [ "$mode" = checked ]; then
		definition=-dCHECKED
	fi
	tpcc_translate $definition \
		-o"$tmp/currency-negative-precision-$mode.cc" \
		tests/currency_negative_precision.pp
	tpcc_build "$tmp/currency-negative-precision-$mode" \
		"$tmp/currency-negative-precision-$mode.cc" \
		"$tmp/system.cc"
	if tpcc_run "$tmp/currency-negative-precision-$mode"; then
		echo "Str accepted a negative Currency precision under $mode range checking" >&2
		exit 1
	else
		status=$?
	fi
	if [ "$status" -ne 201 ]; then
		echo "Str returned $status rather than runtime error 201 for a negative Currency precision under $mode range checking" >&2
		exit 1
	fi
done

echo "Currency type tests passed"
