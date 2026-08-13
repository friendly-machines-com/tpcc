#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/system.cc" rtl/system.pp
tpcc_translate -o"$tmp/old_file_api.cc" \
	tests/old_file_api.pp

for operation in assign rewrite reset close seek filepos filesize eof truncate ioresult blockread blockwrite
do
	if ! grep -Fq "::u_system::p_$operation" \
		"$tmp/old_file_api.cc"
	then
		echo "missing old file operation: $operation" >&2
		exit 1
	fi
done

tpcc_build "$tmp/old_file_api" \
	"$tmp/old_file_api.cc" \
	"$tmp/system.cc"

actual=$(cd "$tmp" &&
	tpcc_run \
	./old_file_api)
expected='2
0
0
4
0
4
0
4
FALSE
TRUE
0
1
4
0
4
10
20
30
40
0'
if test "$actual" != "$expected"
then
	echo "unexpected old file API result" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

echo "old file API tests passed"
