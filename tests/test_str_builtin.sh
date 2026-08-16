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

if tpcc_translate -o"$tmp/text-precision.cc" \
	tests/str_text_precision_rejected.pp \
	>"$tmp/text-precision.stdout" \
	2>"$tmp/text-precision.stderr"; then
	echo "Str accepted a precision qualifier for an existing textual value" >&2
	exit 1
fi
if ! grep -Fq \
	"Str precision requires a predefined real value" \
	"$tmp/text-precision.stderr"; then
	echo "wrong diagnostic for a textual Str precision qualifier" >&2
	cat "$tmp/text-precision.stderr" >&2
	exit 1
fi

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
