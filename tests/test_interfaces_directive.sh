#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/interfaces_directive.cc" \
	tests/interfaces_directive.pp

tpcc_build "$tmp/interfaces_directive" \
	"$tmp/interfaces_directive.cc" \
	"$tmp/system.cc"
"$tmp/interfaces_directive"

for source in \
	tests/interface_default_com_rejected.pp \
	tests/interface_explicit_com_rejected.pp \
	tests/interface_default_reset_rejected.pp \
	tests/interface_push_pop_persistent_rejected.pp \
	tests/interface_directive_invalid.pp
do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid interface-model source: $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! grep -Fq -- "$expected" "$tmp/stderr"
	then
		echo "wrong interface-model diagnostic; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "INTERFACES directive tests passed"
