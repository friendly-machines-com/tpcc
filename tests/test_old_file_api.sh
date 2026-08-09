#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-old-file-api-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/old_file_api.cc" \
	tests/old_file_api.pp

for operation in assign rewrite reset close seek filepos filesize eof truncate ioresult blockread blockwrite
do
	if ! rg -Fq "::u_system::p_$operation" \
		"$tmp/old_file_api.cc"
	then
		echo "missing old file operation: $operation" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/old_file_api.cc" \
	"$tmp/system.cc" \
	-o "$tmp/old_file_api"

actual=$(cd "$tmp" &&
	ASAN_OPTIONS=detect_leaks=1 \
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
