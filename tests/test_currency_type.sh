#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

tpcc_translate -o"$tmp/currency_type.cc" tests/currency_type.pp
# ncon.pas writes value_currency through WriteLn. TPCC's Write lowering must
# retain the concrete Currency Str projection instead of selecting a real or
# catch-all formatter.
grep -Fq \
	'tpcc_make_formatted_value(static_cast<::u_system::t_currency>(p_parsedexact))' \
	"$tmp/currency_type.cc"
tpcc_build "$tmp/currency_type" \
	tests/currency_type_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/currency_type"

tpcc_translate -o"$tmp/currency_range_modes.cc" \
	tests/currency_range_modes.pp
tpcc_build "$tmp/currency_range_modes" \
	tests/currency_range_modes_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/currency_range_modes"

# The two calls resolve the same Currency formal. {$R} changes only the
# selected conversion node: R- materializes low carrier bits, while R+ emits
# the range failure at the call boundary.
grep -Fq 'p_acceptcurrency(::u_system::m_currency_from_raw(' \
	"$tmp/currency_range_modes.cc"
grep -Fq 'p_acceptcurrency(::u_system::m_range_checked_currency_cast(' \
	"$tmp/currency_range_modes.cc"

if tpcc_translate -o"$tmp/currency_checked_constant_rejected.cc" \
	tests/currency_checked_constant_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an out-of-range R+ Currency constant" >&2
	exit 1
fi
grep -Fq "integer constant out of range for Currency" "$tmp/stderr"

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
