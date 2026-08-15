#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/character_code_literals.cc" \
	tests/character_code_literals.pp

tpcc_build "$tmp/character_code_literals" \
	"$tmp/character_code_literals.cc" \
	"$tmp/system.cc"

tpcc_run "$tmp/character_code_literals"

if tpcc_translate \
	-o"$tmp/hex_empty.cc" \
	tests/character_code_hex_empty.pp \
	>"$tmp/hex_empty.out" 2>"$tmp/hex_empty.err"
then
	echo "accepted an empty hexadecimal character-code literal" >&2
	exit 1
fi
if ! grep -Fq \
	'malformed character-code literal: #$' \
	"$tmp/hex_empty.err"
then
	echo "wrong diagnostic for an empty hexadecimal character-code literal" >&2
	sed -n '1,40p' "$tmp/hex_empty.err" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/hex_out_of_range.cc" \
	tests/character_code_hex_out_of_range.pp \
	>"$tmp/hex_out_of_range.out" 2>"$tmp/hex_out_of_range.err"
then
	echo "accepted an out-of-range hexadecimal character-code literal" >&2
	exit 1
fi
if ! grep -Fq \
	'malformed character-code literal: #$100' \
	"$tmp/hex_out_of_range.err"
then
	echo "wrong diagnostic for an out-of-range hexadecimal character-code literal" >&2
	sed -n '1,40p' "$tmp/hex_out_of_range.err" >&2
	exit 1
fi

echo "character-code literal tests passed"
