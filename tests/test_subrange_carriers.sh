#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-Futests/subrange_carrier_units \
	-o"$tmp/subrange_carriers.cc" \
	tests/subrange_carrier_units/subrange_carriers.pp

for required in \
	'struct m_subrange_' \
	'using t_tfirst [[maybe_unused]] = ::u_rangecarrier::m_subrange_' \
	'using t_tsecond [[maybe_unused]] = ::u_rangecarrier::m_subrange_' \
	'using t_tfirstalias [[maybe_unused]] = ::u_rangecarrier::m_subrange_' \
	'std::is_standard_layout_v<m_subrange_' \
	'std::is_trivially_copyable_v<m_subrange_'
do
	if ! rg -Fq "$required" "$tmp/rangecarrier.h"
	then
		echo "missing public subrange carrier output: $required" >&2
		exit 1
	fi
done

signature_count=$(rg -Fc 'p_identify(' "$tmp/rangecarrier.h")
if test "$signature_count" -ne 1
then
	echo "subrange-erased routine emitted an unexpected overload count" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/subrange_overload_rejected.cc" \
	tests/subrange_overload_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted overloads distinguished only by subrange declarations" >&2
	exit 1
fi
if ! rg -Fq 'both declarations have the same Pascal overload signature' "$tmp/stderr"
then
	echo "wrong duplicate-subrange-overload diagnostic" >&2
	sed -n '1,120p' "$tmp/stderr" >&2
	exit 1
fi

tpcc_build "$tmp/subrange_carriers" \
	"$tmp/subrange_carriers.cc" \
	"$tmp/rangecarrier.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/subrange_carriers"

echo "subrange carrier tests passed"
