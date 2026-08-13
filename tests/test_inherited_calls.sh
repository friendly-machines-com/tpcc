#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/inherited_calls.cc" \
	tests/inherited_calls.pp

if ! rg -Fq 't_tbase::p_insert(' \
	"$tmp/inherited_calls.cc"
then
	echo "parenthesized inherited instance call was not emitted" >&2
	exit 1
fi
if ! rg -Fq 't_tbase::p_select(' \
	"$tmp/inherited_calls.cc"
then
	echo "inherited overload was not selected" >&2
	exit 1
fi
if ! rg -Fq 't_tbase::p_withdefault(7ull)' \
	"$tmp/inherited_calls.cc"
then
	echo "inherited call did not materialize a default argument" >&2
	exit 1
fi

tpcc_build "$tmp/inherited_calls" \
	"$tmp/inherited_calls.cc" \
	"$tmp/system.cc"

actual=$("$tmp/inherited_calls")
test "$actual" = 'inherited calls passed'

echo "inherited call tests passed"
