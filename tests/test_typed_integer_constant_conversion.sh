#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/typed_integer_constant_conversion.cc" \
	tests/typed_integer_constant_conversion.pp

# R+ must not emit a range check after semantic constant evaluation has
# already proved that the actual value belongs to the selected destination.
if grep -Fq 'm_range_checked_ordinal_cast' \
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

tpcc_translate \
	-o"$tmp/integer_operator_domain.cc" \
	tests/integer_operator_domain.pp

tpcc_build "$tmp/integer_operator_domain" \
	"$tmp/integer_operator_domain.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/integer_operator_domain"

echo "typed integer constant conversion tests passed"
