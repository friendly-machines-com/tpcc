#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/single_edge.cc" \
	tests/implicit_conversion_single_edge.pp

tpcc_build "$tmp/single_edge" \
	"$tmp/single_edge.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/single_edge"

for kind in RECORD INTEGER NARROW SUBRANGE
do
	if tpcc_translate -d"OMIT_${kind}_DIRECT" \
		-o"$tmp/chained.cc" \
		tests/implicit_conversion_single_edge.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted an implicit A -> B -> C conversion chain: $kind" >&2
		exit 1
	fi

	if ! rg -Fq 'no implicit conversion' "$tmp/stderr"
	then
		echo "wrong diagnostic for rejected implicit conversion chain: $kind" >&2
		sed -n '1,80p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "single-edge implicit conversion tests passed"
