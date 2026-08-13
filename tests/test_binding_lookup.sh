#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/binding_lookup.cc" \
	tests/binding_lookup.pp

tpcc_build "$tmp/binding_lookup" \
	"$tmp/binding_lookup.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/binding_lookup"

if tpcc_translate -o"$tmp/rejected.cc" \
	tests/binding_same_scope_rejected.pp \
	>"$tmp/rejected.out" 2>&1
then
	echo "same-scope type/value duplicate was accepted" >&2
	exit 1
fi
if ! rg -Fq "duplicate identifier: x" \
	"$tmp/rejected.out"
then
	echo "same-scope duplicate produced the wrong diagnostic" >&2
	cat "$tmp/rejected.out" >&2
	exit 1
fi

echo "binding-lookup tests passed"
