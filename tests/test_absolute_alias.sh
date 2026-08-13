#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/system.cc" rtl/system.pp
tpcc_translate -o"$tmp/absolute_alias.cc" tests/absolute_alias.pp
tpcc_build "$tmp/absolute_alias" \
	"$tmp/absolute_alias.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/absolute_alias"

for source in \
	tests/absolute_var_param_rejected.pp \
	tests/absolute_non_pointer_rejected.pp \
	tests/absolute_size_mismatch_rejected.pp
do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid absolute source: $source" >&2
		exit 1
	fi
	expected_error=$(sed -n '1p' "$base.error")
	if ! rg -Fq -- "$expected_error" "$tmp/stderr"
	then
		echo "wrong absolute diagnostic; expected: $expected_error" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "Absolute alias tests passed"
