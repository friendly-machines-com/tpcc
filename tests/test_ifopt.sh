#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/ifopt.cc" tests/ifopt.pp

for selected in \
	p_qplus \
	p_qminusrejected \
	p_outerstatepreserved \
	p_qminus \
	p_rinitiallyminus \
	p_nestedactive
do
	if ! rg -q "\\b${selected}\\b" "$tmp/ifopt.cc"; then
		echo "missing IFOPT-selected declaration: $selected" >&2
		exit 1
	fi
done

if rg -q 'p_wrong' "$tmp/ifopt.cc"; then
	echo "an unselected IFOPT branch was emitted" >&2
	exit 1
fi

tpcc_build "$tmp/ifopt" \
	"$tmp/ifopt.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/ifopt"

if tpcc_translate -o"$tmp/invalid.cc" \
	tests/ifopt_invalid.pp >"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "expected malformed IFOPT to be rejected" >&2
	exit 1
fi
if ! rg -Fq \
	'$ifopt expects one option letter followed by + or -' \
	"$tmp/stderr"
then
	echo "wrong malformed-IFOPT diagnostic" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "IFOPT tests passed"
