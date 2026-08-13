#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/h_directive_strings.cc" \
	tests/h_directive_strings.pp

tpcc_build "$tmp/h_directive_strings" \
	"$tmp/h_directive_strings.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/h_directive_strings"

if tpcc_translate -o"$tmp/longstrings_invalid.cc" \
	tests/longstrings_invalid.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted invalid LONGSTRINGS setting" >&2
	exit 1
fi
if ! grep -Fq \
	"$(sed -n '1p' tests/longstrings_invalid.error)" \
	"$tmp/stderr"
then
	echo "wrong LONGSTRINGS diagnostic" >&2
	sed -n '1,40p' "$tmp/stderr" >&2
	exit 1
fi

echo "H directive string tests passed"
