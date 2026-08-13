#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/operator_argument_matching.cc" \
	tests/operator_argument_matching.pp

tpcc_build "$tmp/operator_argument_matching" \
	"$tmp/operator_argument_matching.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/operator_argument_matching"

if tpcc_translate \
	-o"$tmp/symbol_identifier_rejected.cc" \
	tests/symbol_identifier_rejected.pp \
	>"$tmp/symbol_identifier_rejected.out" 2>&1
then
	echo "accepted an operator symbol as a procedure identifier" >&2
	exit 1
fi

if ! rg -Fq 'error: expected identifier' \
	"$tmp/symbol_identifier_rejected.out"
then
	echo "wrong symbolic-identifier diagnostic" >&2
	cat "$tmp/symbol_identifier_rejected.out" >&2
	exit 1
fi

echo "operator argument matching tests passed"
