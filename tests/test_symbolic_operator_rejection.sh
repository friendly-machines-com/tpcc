#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

for spelling in ampersand pipe
do
	source="tests/${spelling}_operator_rejected.pp"
	if tpcc_translate -o"$tmp/${spelling}.cc" "$source" \
		>"$tmp/${spelling}.stdout" 2>"$tmp/${spelling}.stderr"
	then
		echo "$source unexpectedly compiled" >&2
		exit 1
	fi
	grep -Fq 'unknown input character' "$tmp/${spelling}.stderr"
done
