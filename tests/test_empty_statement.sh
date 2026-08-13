#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/empty_statement.cc" tests/empty_statement.pp

if ! awk '
	/pas_label_emptyatend:/ {
		if (getline > 0 && $0 ~ /^[[:space:]]*\{\}/)
			found = 1
	}
	END { exit !found }
' "$tmp/empty_statement.cc"
then
	echo "empty labeled statement was not preserved in C++20" >&2
	exit 1
fi

tpcc_build "$tmp/empty_statement" \
	-pedantic-errors \
	"$tmp/empty_statement.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/empty_statement"

echo "empty-statement tests passed"
