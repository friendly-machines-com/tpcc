#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/named_text_input.cc" \
	tests/named_text_input.pp

{
	printf 'alpha\r\n'
	awk 'BEGIN { for (i = 0; i < 300; ++i) printf "x"; printf "\n" }'
	printf 'omega\r'
} >"$tmp/input.txt"

tpcc_build "$tmp/named_text_input" \
	"$tmp/named_text_input.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/named_text_input" "$tmp/input.txt"

tpcc_translate -o"$tmp/named_text_output.cc" \
	tests/named_text_output.pp

tpcc_build "$tmp/named_text_output" \
	"$tmp/named_text_output.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/named_text_output" "$tmp/output.txt"
printf 'kept' >"$tmp/expected-output.txt"
cmp "$tmp/expected-output.txt" "$tmp/output.txt"

echo "named Text input/output tests passed"
