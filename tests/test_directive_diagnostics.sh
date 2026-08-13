#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/directive_diagnostics_inactive.cc" \
	tests/directive_diagnostics_inactive.pp
tpcc_build "$tmp/directive_diagnostics_inactive" \
	"$tmp/directive_diagnostics_inactive.cc" \
	"$tmp/system.cc"
"$tmp/directive_diagnostics_inactive"

for source in \
	tests/directive_error.pp \
	tests/directive_fatal.pp \
	tests/directive_error_empty.pp \
	tests/directive_error_include.pp
do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted active diagnostic directive: $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! grep -Fq -- "$expected" "$tmp/stderr"
	then
		echo "wrong directive diagnostic; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "directive diagnostic tests passed"
