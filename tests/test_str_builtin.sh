#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/str_builtin.cc" tests/str_builtin.pp

tpcc_build "$tmp/str_builtin_pascal" \
	"$tmp/str_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/str_builtin_pascal" >"$tmp/pascal-output"
printf '  SparseName\n' >"$tmp/expected-output"
if ! cmp "$tmp/expected-output" "$tmp/pascal-output"; then
	echo "enum Write/WriteLn formatting differs from Str" >&2
	exit 1
fi

tpcc_build "$tmp/str_builtin" \
	tests/str_builtin_runtime.cpp
tpcc_run "$tmp/str_builtin"

for mode in unchecked checked
do
	definition=
	if [ "$mode" = checked ]; then
		definition=-dCHECKED
	fi
	tpcc_translate $definition \
		-o"$tmp/invalid-enum-$mode.cc" \
		tests/str_invalid_enum.pp
	tpcc_build "$tmp/invalid-enum-$mode" \
		"$tmp/invalid-enum-$mode.cc" \
		"$tmp/system.cc"
	if tpcc_run "$tmp/invalid-enum-$mode"; then
		echo "Str accepted an unnamed enum ordinal under $mode range checking" >&2
		exit 1
	else
		status=$?
	fi
	if [ "$status" -ne 107 ]; then
		echo "Str returned $status rather than runtime error 107 for an unnamed enum ordinal under $mode range checking" >&2
		exit 1
	fi
done

echo "Str builtin tests passed"
