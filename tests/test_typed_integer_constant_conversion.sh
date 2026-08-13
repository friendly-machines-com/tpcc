#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/typed_integer_constant_conversion.cc" \
	tests/typed_integer_constant_conversion.pp

# R+ must not emit a range check after semantic constant evaluation has
# already proved that the actual value belongs to the selected destination.
if rg -Fq 'm_range_checked_ordinal_cast' \
	"$tmp/typed_integer_constant_conversion.cc"
then
	echo "range-checked a proved in-range typed integer constant" >&2
	exit 1
fi

tpcc_build "$tmp/typed_integer_constant_conversion" \
	"$tmp/typed_integer_constant_conversion.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/typed_integer_constant_conversion"

if tpcc_translate \
	-o"$tmp/nonconstant.cc" \
	tests/typed_integer_nonconstant_common_domain.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted incomparable runtime Int64/QWord division domains" >&2
	exit 1
fi

rg -Fq "ambiguous overload for 'uncheckedintdivide'" "$tmp/stderr"
rg -Fq 'conflicting argument preferences:' "$tmp/stderr"

echo "typed integer constant conversion tests passed"
